#pragma once

#include "util.h"
#include <string.h>
#include <functional>
#include <sstream>
#include <mutex>
#include <atomic>
#include "BufferedWrite.h"

extern ArmaCallback callback;

template<class HandleType>
class ReadWriteHandler
{
private:
	//mutex for working on the read thread. probably not necessary, but just in case
	std::mutex readThreadMutex;
	std::thread* readThread = nullptr;
	//used to terminate the read thread
	std::atomic<bool> usingReadThread = true;

	//mutex for working on the write thread. probably not necessary, but just in case
	std::mutex writeThreadMutex;
	std::thread* writeThread = nullptr;
	//whether we should be using the write thread
	std::atomic<bool> usingWriteThread = false;

	//the next bit of data the write thread hasn't gotten to yet (head of the linked list)
	BufferedWrite* toWrite = nullptr;

	//the most recent write queued (tail of the linked list)
	BufferedWrite* lastWrite = nullptr;

	//the file handle to the serial port
	HandleType* handle;
	//the communication method that owns this handler (so we can get its id)
	ICommunicationMethod* commMethod;

	//mutex which must be held to write to the serial port
	std::mutex writeMutex;
	//writes a string to the handle
	std::function<bool(HandleType*, char*, DWORD, DWORD*)> writeString;
	//reads one char from the handle
	std::function<bool(HandleType*, char*)> readOneChar;

	//whether a read thread should be used
	bool useReadThread;
	//whether a write thread should be used
	bool useWriteThread;

	std::mutex callbackOptionsMutex;
	//must be used only when callbackOptionsMutex is held because it's likely reads and writes will be
	//both staggered (e.g. reading one field then the other) and, in the case of a single copy of the
	//whole struct, non-atomic
	//should only be used directly in its getter and setter
	ReadCallbackOptions callbackOptions;

public:
	ReadWriteHandler(HandleType* handle, ICommunicationMethod* commMethod, std::function<bool(HandleType*, char*, DWORD, DWORD*)> writeString,
		std::function<bool(HandleType*, char*)> readOneChar, bool useReadThread, bool useWriteThread) {
		this->handle = handle;
		this->commMethod = commMethod;
		this->writeString = writeString;
		this->readOneChar = readOneChar;
		this->useReadThread = useReadThread;
		this->useWriteThread = useWriteThread;
		this->lastWrite = this->toWrite = new BufferedWrite(nullptr, 0);
		this->toWrite->dummy = true;
		this->callbackOptions.type = ReadCallbackTypes::ON_CHAR;
		this->callbackOptions.value.onChar = '\n';
	}
	void write(std::string& data, std::stringstream& out) {
		if (usingWriteThread.load()) {
			{
				std::unique_lock<std::mutex> l(lastWrite->nextLock);
				lastWrite->next = new BufferedWrite(data.c_str(), (DWORD)data.length());
			}
			lastWrite->cond.notify_all();
			lastWrite = lastWrite->next;
			sendSuccessArr(out, "Write successfully queued");
		}
		else {
			DWORD written;
			std::unique_lock<std::mutex> lock(writeMutex);
			if (!this->writeString(handle, (char*)data.c_str(), (DWORD)data.length(), &written)) {
				DWORD err = GetLastError();
				sendFailureArr(out, "Error while writing: " + formatErr(err));
			}
			else {
				sendSuccessArr(out, "Successfully wrote " + std::to_string(written) + " bytes");
			}
		}
	}
	void startThreads() {
		//start up the read thread
		if (useReadThread) {
			std::unique_lock<std::mutex> lock(readThreadMutex);
			this->usingReadThread = true;
			readThread = new std::thread([](ReadWriteHandler<HandleType>* rwh) { rwh->readThreadFunction(); }, this);
			
		}
		if (useWriteThread) {
			std::unique_lock<std::mutex> lock(this->writeThreadMutex);
			this->usingWriteThread = true;
			writeThread = new std::thread([](ReadWriteHandler<HandleType>* rwh) { rwh->writeThreadFunction(); }, this);
			
		}
	}
	void setCallbackOptions(ReadCallbackOptions val) {
		callbackOptionsMutex.lock();
		this->callbackOptions = val;
		callbackOptionsMutex.unlock();
	}
	void callbackOnLength(int length) {
		callbackOptionsMutex.lock();
		this->callbackOptions.type = ReadCallbackTypes::ON_LENGTH;
		this->callbackOptions.value.onLength = length;
		callbackOptionsMutex.unlock();
	}
	void callbackOnChar(char c) {
		callbackOptionsMutex.lock();
		this->callbackOptions.type = ReadCallbackTypes::ON_CHAR;
		this->callbackOptions.value.onChar = c;
		callbackOptionsMutex.unlock();
	}
	ReadCallbackOptions getCallbackOptions() {
		callbackOptionsMutex.lock();
		auto ans = this->callbackOptions;
		callbackOptionsMutex.unlock();
		return ans;
	}
	void enableWriteThread(std::stringstream& out) {
		this->useWriteThread = true;
		std::unique_lock<std::mutex> lock(this->writeThreadMutex);
		if (usingWriteThread.load()) {
			sendFailureArr(out, "Already using write thread");
			return;
		}
		usingWriteThread = true;
		writeThread = new std::thread([](ReadWriteHandler<HandleType>* rwh) { rwh->writeThreadFunction(); }, this);
		sendSuccessArr(out, "Write thread activated");
	}
	void disableWriteThread(std::stringstream& out) {
		{
			std::unique_lock<std::mutex> lock(this->writeThreadMutex);
			this->usingWriteThread = false;
		}
		this->writeThread->join();
		sendSuccessArr(out, "Successfully terminated write thread");
		delete this->writeThread;
		this->writeThread = nullptr;
	}
	void readThreadFunction() {
		std::stringstream line;
		char readChar[20]; //only needs to be 1, marking 20 just in case
		bool readOk;
		//We need to buffer the strings we send to arma because I don't know if Arma
		//copies the string data when it writes it to its internal callback buffer,
		//or when it gets removed from that buffer and used in SQF, and if it's
		//the latter we could end up with nasty stuff.
		//The idea here is that we have a circular buffer of 101 strings, and since 
		//ARMA's internal buffer for callback data is only 100 long, we can safely assume
		//that any string older than that is done for (and we also have to account for
		//the "current" string which may be rejected do to a full buffer).
		//There's a good chance Arma is doing the right thing and copying the strings
		//right away, especially since it *seems* to work ok without the buffer,
		//but until I go do the work to find out this'll do ok
		std::string* buffd[101];
		for (int i = 0; i < 101; i++) buffd[i] = nullptr;
		int ind = 0;
		size_t charsRead = 0;
		ReadCallbackOptions prevOptions = getCallbackOptions();
		std::function<void(std::string, std::string)> send = [&](std::string id, std::string str) {
			if (buffd[ind] != nullptr) delete (buffd[ind]);
			buffd[ind] = new std::string("[\"" + id + "\", \"" + str + "\"]");
			std::string* ans = buffd[ind++];
			if (ind == 101) ind = 0;
			//callback should never be nullptr at this point. if it is, something
			//has gone seriously wrong
			while (callback("ArmaCOM", "data_read", ans->c_str()) == -1) {
				//if the callback returns -1, the game's buffer for callback data is full,
				//so we have to wait until that's cleared. Which may be never.
				//Oh well.
				Sleep(1);
			}
		};
		while (true) {
			auto cbo = getCallbackOptions();
			//if callbackOptions has changed, make sure the existing buffer doesn't have data that should
			//be sent to Arma by the rules of the new callbackOptions
			if (prevOptions != cbo) {
				if (cbo.type == ReadCallbackTypes::ON_CHAR) {
					char cbOn = cbo.value.onChar;
					auto str = line.str();
					auto it = str.begin();
					auto end = str.end();
					line.str("");
					charsRead = 0;
					for (; it < end; it++) {
						if (*it == cbOn) {
							send(commMethod->getID(), line.str());
							line.str("");
							charsRead = 0;
						}
						else {
							line << *it;
							charsRead++;
						}
					}
				}
				else if (cbo.type == ReadCallbackTypes::ON_LENGTH) {
					auto cbOn = cbo.value.onLength;
					if (charsRead >= cbOn) {
						auto str = line.str();
						line.str("");
						for (size_t i = 0; i <= charsRead - cbOn; i += cbOn) {
							send(commMethod->getID(), str.substr(i, cbOn));
						}
						auto leftover = charsRead % cbOn;
						if (leftover == 0) {
							charsRead = 0;
						}
						else {
							line << str.substr(charsRead - leftover, leftover);
							charsRead = leftover;
						}
					}
				}
				else {
					throw "Unimplemented ReadCallbackType in ReadWriteHandler read thread";
				}
			}
			prevOptions = cbo;
			//this may fail but there's nothing we can really do, so....
			if (readOk = this->readOneChar(this->handle, readChar)) {
				if (cbo.type != ReadCallbackTypes::ON_CHAR || readChar[0] != cbo.value.onChar) {
					line << readChar[0];
					charsRead++;
				}
			}
			bool shouldCallback = false;
			if (this->callbackOptions.type == ReadCallbackTypes::ON_CHAR && readOk) {
				shouldCallback = readChar[0] == cbo.value.onChar;
			}
			else if (this->callbackOptions.type == ReadCallbackTypes::ON_LENGTH) {
				shouldCallback = (charsRead >= cbo.value.onLength);
			}
			if (shouldCallback) {
				send(commMethod->getID(), line.str());
				line.str("");
				charsRead = 0;
			}
			if (!usingReadThread.load()) return;
		}
	}
	void writeThreadFunction() {
		DWORD written;
		while (true) {
			if (!this->toWrite->dummy) {
				//this may fail, since we're doing threaded writes there's not really anything
				//we can do about it
				writeMutex.lock();
				this->writeString(handle, this->toWrite->data, this->toWrite->dataSize, &written);
				writeMutex.unlock();
			}
			//if next is null, wait for the notification that it's not null anymore
			{
				auto lock = std::unique_lock<std::mutex>(toWrite->nextLock);
				while (toWrite->next == nullptr) {
					toWrite->cond.wait_for(lock, std::chrono::seconds(1));
					//this code is not currently used
					//since write threads can't be turned off once they're turned on right now,
					//but I'm putting it here so I know what's up if I change that later
					if (!usingWriteThread.load()) {
						//clear out the rest of the write buffer
						while (toWrite->next != nullptr) {
							auto prev = toWrite;
							toWrite = toWrite->next;
							delete prev;
							if (!toWrite->dummy) {
								DWORD written;
								writeMutex.lock();
								auto wrote = this->writeString(handle, this->toWrite->data, this->toWrite->dataSize, &written);
								writeMutex.unlock();
							}
						}
						return;
					}
				}
			}
			//free the old toWrite and replace it with the next queued write
			auto prev = toWrite;
			toWrite = toWrite->next;
			delete prev;
		}
	}
	void stopThreads() {
		if (this->usingWriteThread) {
			std::unique_lock<std::mutex> lock(this->writeThreadMutex);
			this->usingWriteThread = false;
			this->writeThread->join();
			delete this->writeThread;
			this->writeThread = nullptr;
		}
		if (this->usingReadThread) {
			std::unique_lock<std::mutex> lock(this->readThreadMutex);
			this->usingReadThread = false;
			this->readThread->join();
			delete this->readThread;
			this->readThread = nullptr;
		}
	}
};

