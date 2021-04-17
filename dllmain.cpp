// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
//TODO: add data bits
//TODO: make changes to settings affect opened ports
#include <iostream>
#include <cstring>
#include <sstream>
#include <string.h>
#include <mutex>
#include <atomic>
#include <map>

static const std::string portPrefix = "\\\\.\\";

//this macro checks if a COM port was specified in argv[0] and if it was, places it in `port`. If not, it errors out with `goto end`.
#define GET_PORT \
	SerialPort* port; \
	if (argc == 0) { ans << "You must specify a port for this command"; goto end; } \
	if (QueryDosDeviceA(argv[0].c_str(), pathBuffer, 800) == 0) { \
		ans << "The specified port (\"" << argv[0] << "\") is invalid"; \
		goto end; \
	} \
	if (ports.count(argv[0]) == 0) { \
		port = new SerialPort(argv[0]); \
		ports[argv[0]] =  port; \
	} \
	else { \
		port = ports[argv[0]]; \
	}

//the callback function Arma gives us
static ArmaCallback callback = nullptr;

//arrays used when users select parameters for the serial port
//baud rate
static const int numRates = 14;
static const char* const baudNames[] = {
	"110",
	"300",
	"600",
	"1200",
	"2400",
	"4800",
	"9600",
	"14400",
	"19200",
	"38400",
	"57600",
	"115200",
	"128000",
	"256000"
};
static const DWORD baudRates[] = {
	CBR_110,
	CBR_300,
	CBR_600,
	CBR_1200,
	CBR_2400,
	CBR_4800,
	CBR_9600,
	CBR_14400,
	CBR_19200,
	CBR_38400,
	CBR_57600,
	CBR_115200,
	CBR_128000,
	CBR_256000
};
//parity bits
static const int numParities = 5;
static const char* const parityNames[] = {
	"No parity",
	"Odd parity",
	"Even parity",
	"Mark parity",
	"Space parity"
};
static const BYTE parities[] = {
	NOPARITY,
	ODDPARITY,
	EVENPARITY,
	MARKPARITY,
	SPACEPARITY
};
//stop bits
static const int numStopBits = 3;
static const char* const stopBitNames[] = {
	"1 stop bit",
	"1.5 stop bits",
	"2 stop bits"
};
static int stopBitValues[] = {
	ONESTOPBIT,
	ONE5STOPBITS,
	TWOSTOPBITS
};
//data bits/byte size
static const int numDataBits = 5;
static const char* const dataBitNames[] = {
	"4 data bits",
	"5 data bits",
	"6 data bits",
	"7 data bits",
	"8 data bits"
};
static const BYTE dataBitValues[] = {
	4,
	5,
	6,
	7,
	8
};

//takes a Windows error code (from GetLastError) and returns a string with the error message
std::string formatErr(int err) {
	char buff[500];
	FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buff,
		500,
		nullptr
	);
	return std::string(buff);
}

//allows writes to be queued for another thread to use later simple singly-linked list
//this is written using a lockless design but uses locks as the next-best thing to busy waiting, latency-wise
//this class, along with the code using it, is designed to only be used by two threads (one write, and one read).
//to allow for more read threads, both the design of the class and the code using it must be changed.
//to allow for more write threads, only the design of the other write threads must be changed, though other changes may make it simpler.
class BufferedWrite {
public:
	//mutex used for listening for a change to `next` using `cond`
	std::mutex nextLock;
	//condition variable for listening to changes to `next`
	std::condition_variable cond;
	//linkedlist next value. may be null.
	std::atomic<BufferedWrite*> next;
	//cstring containing data to write. may be null if and only if `dummy` is true. may not be null even if `dummy` is true.
	char* data;
	//length of `data`
	DWORD dataSize;
	//if true, this node contains no data and exists only to make multithreading easier.
	bool dummy;
	BufferedWrite(const char* data, DWORD dataSize) {
		this->data = new char[(size_t)dataSize + 2];
		if (dataSize != 0) strncpy_s(this->data, (rsize_t)dataSize + 2, data, (rsize_t)dataSize + 1);
		this->dataSize = dataSize;
		this->next = nullptr;
		this->dummy = false;
	}
	~BufferedWrite() {
		delete data;
	}
};

//simple wrapper class for Windows' intrinsic serial port API
class SerialPort {
private:
	//the port name as it's passed to the constructor
	std::string portNamePretty;
	//the port name as Windows sees it (prefixed by backslashes etc)
	std::string portName;

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
	HANDLE serialPort = nullptr;

	//mutex which must be held to write to the serial port
	std::mutex writeMutex;

	//DCB params
	int baudRate = 6;
	int parity = 0;
	int stopBits = 0;
	int dataBits = 4;
	bool fParity = false;
	bool fOutxCtsFlow = false;
	bool fOutxDsrFlow = false;
	int fDtrControl = 1;
	bool fDsrSensitivity = false;
	bool fTXContinueOnXoff = false;
	bool fOutX = false;
	bool fInX = false;
	bool fErrorChar = false;
	bool fNull = false;
	int fRtsControl = 1;
	bool fAbortOnError = false;
	WORD XonLim = 2048;
	WORD XoffLim = 512;
	char XonChar = 17;
	char XoffChar = 19;
	char ErrorChar = 0;
	char EofChar = 0;
	char EvtChar = 0;

public:
	SerialPort(std::string portName) {
		this->portName = portPrefix + portName;
		this->portNamePretty = portName;
	}

	//the loop executed by the write thread for this serial port, if one exists
	void writeThreadFunc() {
		DWORD written;
		while (true) {
			if (!toWrite->dummy) {
				//this may fail, since we're doing threaded writes there's not really anything
				//we can do about it
				writeMutex.lock();
				WriteFile(serialPort, toWrite->data, toWrite->dataSize, &written, nullptr);
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
								writeMutex.lock();
								WriteFile(serialPort, toWrite->data, toWrite->dataSize, &written, nullptr);
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

	//the function executed by the read thread for this serial port
	void readThreadFunc() {
		std::stringstream line;
		char readChar[20]; //only needs to be 1, marking 20 just in case
		DWORD charsRead;
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
		while (true) {
			//this may fail but there's nothing we can really do, so....
			if (ReadFile(serialPort, readChar, 1, &charsRead, nullptr)) {
				if (charsRead == 1) line << readChar[0];
			}
			if (charsRead == 1 && readChar[0] == '\n') {
				if (buffd[ind] != nullptr) delete (buffd[ind]);
				buffd[ind] = new std::string("[\"" + portNamePretty + "\", \"" + line.str() + "\"]");
				std::string* ans = buffd[ind++];
				if (ind == 101) ind = 0;
				line.str(std::string());
				//callback should never be nullptr at this point. if it is, something
				//has gone seriously wrong
				while (callback("ArmaCOM", "data_read", ans->c_str()) == -1) {
					//if the callback returns -1, the game's buffer for callback data is full,
					//so we have to wait until that's cleared. Which may be never.
					//Oh well.
					Sleep(1);
				}
			}
			if (!usingReadThread.load()) return;
		}
	}

	bool isConnected() {
		return serialPort != nullptr;
	}

	//sets the parameters for the serial port according to instance variables and prints errors to `out`.
	//returns false on a failed Windows API call, and true otherwise.
	bool setParams(std::stringstream& out) {
		DCB serialParams = { 0 };
		serialParams.DCBlength = sizeof(serialParams);
		if (!GetCommState(serialPort, &serialParams)) {
			int err = GetLastError();
			out << "Error while getting comm state: " << formatErr(err);
			return false;
		}
		serialParams.BaudRate = baudRates[baudRate];
		serialParams.ByteSize = dataBitValues[dataBits];
		serialParams.StopBits = stopBitValues[stopBits];
		serialParams.Parity = parities[parity];
		serialParams.fParity = fParity;
		serialParams.fOutxCtsFlow = fOutxCtsFlow;
		serialParams.fOutxDsrFlow = fOutxDsrFlow;
		serialParams.fDtrControl = fDtrControl;
		serialParams.fDsrSensitivity = fDsrSensitivity;
		serialParams.fTXContinueOnXoff = fTXContinueOnXoff;
		serialParams.fOutX = fOutX;
		serialParams.fInX = fInX;
		serialParams.fErrorChar = fErrorChar;
		serialParams.fNull = fNull;
		serialParams.fRtsControl = fRtsControl;
		serialParams.fAbortOnError = fAbortOnError;
		serialParams.XonLim = XonLim;
		serialParams.XoffLim = XoffLim;
		serialParams.XonChar = XonChar;
		serialParams.XoffChar = XoffChar;
		serialParams.ErrorChar = ErrorChar;
		serialParams.EofChar = EofChar;
		serialParams.EvtChar = EvtChar;
		if (!SetCommState(serialPort, &serialParams)) {
			DWORD err = GetLastError();
			out << "Error while setting comm state: " << formatErr(err);
			return false;
		}
		return true;
	}

	//attempts to connect to the serial port specified by instance variables, sets its parameters using `setParams`, and starts applicable read/write threads
	//prints error messages or a success message to `out`.
	void connect(std::stringstream& out) {
		if (serialPort != nullptr) {
			out << "Already connected, please disconnect first";
			return;
		}
		serialPort = CreateFileA(
			portName.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			0,
			0,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			0
		);
		if (serialPort == INVALID_HANDLE_VALUE) {
			DWORD err = GetLastError();
			out << "Error while creating serial port: " << formatErr(err);
			out << portName.c_str();
			serialPort = nullptr;
			return;
		}

		if (!setParams(out)) {
			return;
		}

		COMMTIMEOUTS timeouts = { 0 };
		timeouts.ReadIntervalTimeout = 10;
		timeouts.ReadTotalTimeoutConstant = 10;
		timeouts.ReadTotalTimeoutMultiplier = 10;
		timeouts.WriteTotalTimeoutConstant = 10;
		timeouts.WriteTotalTimeoutMultiplier = 10;
		if (!SetCommTimeouts(serialPort, &timeouts)) {
			DWORD err = GetLastError();
			out << "Error while setting timeouts: " << formatErr(err);
			return;
		}

		//start up the read thread
		{
			std::unique_lock<std::mutex> lock(readThreadMutex);
			usingReadThread = true;
			readThread = new std::thread([](SerialPort* sp) { sp->readThreadFunc(); }, this);
		}
		out << "Succesfully connected to serial port";
	}

	//attempts to disconnect from the connected serial port, and then tells the read/write threads to shut down
	//prints error/success messages to `out`
	void disconnect(std::stringstream& out) {
		if (!CloseHandle(serialPort)) {
			DWORD err = GetLastError();
			out << "Error while disconnecting: " << formatErr(err);
		}
		else {
			out << "Disconnected successfully";
		}
		std::unique_lock<std::mutex> lock(readThreadMutex);
		readThread = nullptr;
		usingReadThread = false;
		serialPort = nullptr;
	}

	//attempts to start a write thread. may be called before the serial port is connected.
	//prints error/success messages to `out`.
	void useWriteThread(std::stringstream& out) {
		writeThreadMutex.lock();
		if (usingWriteThread.load() || writeThread != nullptr) {
			writeThreadMutex.unlock();
			out << "Write thread already active";
			return;
		}
		if (toWrite == nullptr) {
			lastWrite = toWrite = new BufferedWrite(nullptr, 0);
			toWrite->dummy = true;
		}
		usingWriteThread = true;
		writeThread = new std::thread([](SerialPort* sp) { sp->writeThreadFunc(); }, this);
		writeThreadMutex.unlock();
		out << "Write thread activated";
	}

	//attempts to write `data` to the serial port, using the write thread if enabled.
	//prints error/success messages to `out`. can not detect whether a threaded write failed.
	void write(std::string data, std::stringstream& out) {
		if (usingWriteThread.load()) {
			{
				std::unique_lock<std::mutex> l(lastWrite->nextLock);
				lastWrite->next = new BufferedWrite(data.c_str(), (DWORD)data.length());
			}
			lastWrite->cond.notify_all();
			lastWrite = lastWrite->next;
			out << "Write successfully queued";
		}
		else {
			DWORD written;
			std::unique_lock<std::mutex> lock(writeMutex);
			if (!WriteFile(serialPort, data.c_str(), (DWORD)data.length(), &written, nullptr)) {
				DWORD err = GetLastError();
				out << "Error while writing to serial port: " << formatErr(err);
			}
			else {
				out << "Successfully wrote " << written << "bytes";
			}
		}
	}

	bool isUsingWriteThread() {
		return usingWriteThread.load();
	}
	int getBaudRate() {
		return baudRate;
	}
	int getParity() {
		return parity;
	}
	int getStopBits() {
		return stopBits;
	}
	int getDataBits() {
		return dataBits;
	}
	bool getFParity() {
		return fParity;
	}
	bool getFOutxCtsFlow() {
		return fOutxCtsFlow;
	}
	bool getFOutxDsrFlow() {
		return fOutxDsrFlow;
	}
	int getFDtrControl() {
		return fDtrControl;
	}
	bool getFDsrSensitivity() {
		return fDsrSensitivity;
	}
	bool getFTXContinueOnXoff() {
		return fTXContinueOnXoff;
	}
	bool getFOutX() {
		return fOutX;
	}
	bool getFInX() {
		return fInX;
	}
	bool getFErrorChar() {
		return fErrorChar;
	}
	bool getFNull() {
		return fNull;
	}
	int getFRtsControl() {
		return fRtsControl;
	}
	bool getFAbortOnError() {
		return fAbortOnError;
	}
	int getXonLim() {
		return XonLim;
	}
	int getXoffLim() {
		return XoffLim;
	}
	int getXonChar() {
		return (int)XonChar;
	}
	int getXoffChar() {
		return (int)XoffChar;
	}
	int getErrorChar() {
		return (int)ErrorChar;
	}
	int getEofChar() {
		return (int)EofChar;
	}
	int getEvtChar() {
		return (int)EvtChar;
	}
	void setBaudRate(int newBaudRate, std::stringstream& out) {
		if (newBaudRate < 0 || newBaudRate >= numRates) {
			out << "New baud rate out of range";
		}
		else {
			baudRate = newBaudRate;
			if (isConnected()) {
				setParams(out);
			}
		}
	}
	void setParity(int newParity, std::stringstream& out) {
		if (newParity < 0 || newParity >= numParities) {
			out << "New parity out of range";
		}
		else {
			parity = newParity;
			if (isConnected()) {
				setParams(out);
			}
		}
	}
	void setStopBits(int newStopBits, std::stringstream& out) {
		if (newStopBits < 0 || newStopBits >= numStopBits) {
			out << "New stop bits out of range";
		}
		else {
			stopBits = newStopBits;
			if (isConnected()) {
				setParams(out);
			}
		}
	}
	void setDataBits(int newDataBits, std::stringstream& out) {
		if (newDataBits < 0 || newDataBits >= numDataBits) {
			out << "New data bits out of range";
		}
		else {
			dataBits = newDataBits;
			if (isConnected()) {
				setParams(out);
			}
		}
	}
	void setFParity(bool newFParity, std::stringstream& out) {
		fParity = newFParity;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setFOutxCtsFlow(bool newFOutxCtsFlow, std::stringstream& out) {
		fOutxCtsFlow = newFOutxCtsFlow;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setFOutxDsrFlow(bool newFOutxDsrFlow, std::stringstream& out) {
		fOutxDsrFlow = newFOutxDsrFlow;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setFDtrControl(int newFDtrControl, std::stringstream& out) {
		fDtrControl = newFDtrControl;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setFDsrSensitivity(bool newFDsrSensitivity, std::stringstream& out) {
		fDsrSensitivity = newFDsrSensitivity;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setFTXContinueOnXoff(bool newFTXContinueOnXoff, std::stringstream& out) {
		fTXContinueOnXoff = newFTXContinueOnXoff;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setFOutX(bool newFOutX, std::stringstream& out) {
		fOutX = newFOutX;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setFInX(bool newFInX, std::stringstream& out) {
		fInX = newFInX;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setFErrorChar(bool newFErrorChar, std::stringstream& out) {
		fErrorChar = newFErrorChar;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setFNull(bool newFNull, std::stringstream& out) {
		fNull = newFNull;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setFRtsControl(int newFRtsControl, std::stringstream& out) {
		fRtsControl = newFRtsControl;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setFAbortOnError(bool newFAbortOnError, std::stringstream& out) {
		fAbortOnError = newFAbortOnError;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setXonLim(WORD newXonLim, std::stringstream& out) {
		XonLim = newXonLim;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setXoffLim(WORD newXoffLim, std::stringstream& out) {
		XoffLim = newXoffLim;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setXonChar(char newXonChar, std::stringstream& out) {
		XonChar = newXonChar;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setXoffChar(char newXoffChar, std::stringstream& out) {
		XoffChar = newXoffChar;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setErrorChar(char newErrorChar, std::stringstream& out) {
		ErrorChar = newErrorChar;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setEofChar(char newEofChar, std::stringstream& out) {
		EofChar = newEofChar;
		if (isConnected()) {
			setParams(out);
		}
	}
	void setEvtChar(char newEvtChar, std::stringstream& out) {
		EvtChar = newEvtChar;
		if (isConnected()) {
			setParams(out);
		}
	}
};

//maps port names to SerialPorts to allow multiple simultaneous connections
static std::map<std::string, SerialPort*> ports;

//called when the extension is loaded by ARMA
void __stdcall RVExtensionVersion(char* output, int outputSize)
{
	strncpy_s(output, outputSize, "ArmaCOM v0.2", (size_t)outputSize - 1);
}

//called when the extension is loaded by ARMA
void __stdcall RVExtensionRegisterCallback(ArmaCallback c) {
	callback = c;
}

//called when the extension is called with arguments
int __stdcall RVExtensionArgs(char* output, int outputSize, const char* functionC, const char** argvC, int argc)
{
	//used for QueryDosDevice
	char pathBuffer[800];
	std::string function(functionC);
	std::stringstream ans;
	std::string* argv{ new std::string[argc]{} };
	for (int i = 0; i < argc; i++) {
		argv[i] = std::string(argvC[i]);
		std::string& tmp = argv[i];
		//copied from https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
		tmp.erase(tmp.begin(), std::find_if(tmp.begin(), tmp.end(), [](unsigned char ch) {
			return ch != '"';
		}));
		tmp.erase(std::find_if(tmp.rbegin(), tmp.rend(), [](unsigned char ch) {
			return ch != '"';
		}).base(), tmp.end());
	}
	

	//i'm kind of playing with fire here using GOTOs (especially in threaded code)
	//but this is side-project code that doesn't really matter so it's the nicest-looking
	//way to handle this stuff without more complexity. much better than nesting several ifs deep.
	if (function == "listPorts") {
		bool found = false;
		//loop over possible com ports, because the alternative is reading registry keys
		for (int i = 0; i < 255; i++) {
			std::string portName = "COM" + std::to_string(i);
			DWORD comTest = QueryDosDeviceA(portName.c_str(), pathBuffer, 800);
			if (comTest != 0) {
				if (found) ans << ", ";
				else found = true;
				ans << portName << ": " << pathBuffer;
			}
		}
		if (!found) ans << "No COM ports found";
	}
	else if (function == "listBaudRates") {
		for (int i = 0; i < numRates; i++) {
			if (i != 0) ans << ", ";
			ans << i << ": " << baudNames[i];
		}
	}
	else if (function == "listStopBits") {
		for (int i = 0; i < numStopBits; i++) {
			if (i != 0) ans << ", ";
			ans << i << ": " << stopBitNames[i];
		}
	}
	else if (function == "listParities") {
		for (int i = 0; i < numParities; i++) {
			if (i != 0) ans << ", ";
			ans << i << ": " << parityNames[i];
		}
	}
	else if (function == "listDataBits") {
		for (int i = 0; i < numDataBits; i++) {
			if (i != 0) ans << ", ";
			ans << i << ": " << dataBitNames[i];
		}
	}
	else if (function == "getBaudRate") {
		GET_PORT;
		ans << port->getBaudRate();
	}
	else if (function == "getParity") {
		GET_PORT;
		ans << port->getParity();
	}
	else if (function == "getStopBits") {
		GET_PORT;
		ans << port->getStopBits();
	}
	else if (function == "getDataBits") {
		GET_PORT;
		ans << port->getDataBits();
	}
	else if (function == "getFParity") {
		GET_PORT;
		ans << port->getFParity();
	}
	else if (function == "getFOutxCtsFlow") {
		GET_PORT;
		ans << port->getFOutxCtsFlow();
	}
	else if (function == "getFOutxDsrFlow") {
		GET_PORT;
		ans << port->getFOutxDsrFlow();
	}
	else if (function == "getFDtrControl") {
		GET_PORT;
		ans << port->getFDtrControl();
	}
	else if (function == "getFDsrSensitivity") {
		GET_PORT;
		ans << port->getFDsrSensitivity();
	}
	else if (function == "getFTXContinueOnXoff") {
		GET_PORT;
		ans << port->getFTXContinueOnXoff();
	}
	else if (function == "getFOutX") {
		GET_PORT;
		ans << port->getFOutX();
	}
	else if (function == "getFInX") {
		GET_PORT;
		ans << port->getFInX();
	}
	else if (function == "getFErrorChar") {
		GET_PORT;
		ans << port->getFErrorChar();
	}
	else if (function == "getFNull") {
		GET_PORT;
		ans << port->getFNull();
	}
	else if (function == "getFRtsControl") {
		GET_PORT;
		ans << port->getFRtsControl();
	}
	else if (function == "getFAbortOnError") {
		GET_PORT;
		ans << port->getFAbortOnError();
	}
	else if (function == "getXonLim") {
		GET_PORT;
		ans << port->getXonLim();
	}
	else if (function == "getXoffLim") {
		GET_PORT;
		ans << port->getXoffLim();
	}
	else if (function == "getXonChar") {
		GET_PORT;
		ans << port->getXonChar();
	}
	else if (function == "getXoffChar") {
		GET_PORT;
		ans << port->getXoffChar();
	}
	else if (function == "getErrorChar") {
		GET_PORT;
		ans << port->getErrorChar();
	}
	else if (function == "getEofChar") {
		GET_PORT;
		ans << port->getEofChar();
	}
	else if (function == "getEvtChar") {
		GET_PORT;
		ans << port->getEvtChar();
	}
	else if (function == "isUsingWriteThread") {
		GET_PORT;
		ans << port->isUsingWriteThread();
	}
	else if (function == "setBaudRate") {
		try {
			GET_PORT;
			int val = std::stoi(argv[1]);
			port->setBaudRate(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setParity") {
		try {
			GET_PORT;
			int val = std::stoi(argv[1]);
			port->setParity(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setStopBits") {
		try {
			GET_PORT;
			int val = std::stoi(argv[1]);
			port->setStopBits(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setDataBits") {
		try {
			GET_PORT;
			int val = std::stoi(argv[1]);
			port->setDataBits(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setFParity") {
		GET_PORT;
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		port->setFParity(val, ans);
	}
	else if (function == "setFOutxCtsFlow") {
		GET_PORT;
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		port->setFOutxCtsFlow(val, ans);
	}
	else if (function == "setFOutxDsrFlow") {
		GET_PORT;
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		port->setFOutxDsrFlow(val, ans);
	}
	else if (function == "setFDtrControl") {
		GET_PORT;
		int val;
		if (argv[1] == "0") val = 0;
		else if (argv[1] == "1") val = 1;
		else if (argv[1] == "2") val = 2;
		else {
			ans << "Expected 0, 1, or 2";
			goto end;
		}
		port->setFDtrControl(val, ans);
	}
	else if (function == "setFDsrSensitivity") {
		GET_PORT;
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		port->setFDsrSensitivity(val, ans);
	}
	else if (function == "setFTXContinueOnXoff") {
		GET_PORT;
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		port->setFTXContinueOnXoff(val, ans);
	}
	else if (function == "setFOutX") {
		GET_PORT;
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		port->setFOutX(val, ans);
	}
	else if (function == "setFInX") {
		GET_PORT;
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		port->setFInX(val, ans);
	}
	else if (function == "setFErrorChar") {
		GET_PORT;
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		port->setFErrorChar(val, ans);
	}
	else if (function == "setFNull") {
		GET_PORT;
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		port->setFNull(val, ans);
	}
	else if (function == "setFRtsControl") {
		GET_PORT;
		int val;
		if (argv[1] == "0") val = 0;
		else if (argv[1] == "1") val = 1;
		else if (argv[1] == "2") val = 2;
		else if (argv[1] == "3") val = 3;
		else {
			ans << "Expected 0, 1, 2, or 3";
			goto end;
		}
		port->setFRtsControl(val, ans);
	}
	else if (function == "setFAbortOnError") {
		GET_PORT;
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		port->setFAbortOnError(val, ans);
	}
	else if (function == "setXonLim") {
		try {
			GET_PORT;
			int val = std::stoi(argv[1]);
			port->setXonLim(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setXoffLim") {
		try {
			GET_PORT;
			int val = std::stoi(argv[1]);
			port->setXoffLim(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setXonChar") {
		try {
			GET_PORT;
			int val = std::stoi(argv[1]);
			port->setXonChar(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setXoffChar") {
		try {
			GET_PORT;
			int val = std::stoi(argv[1]);
			port->setXoffChar(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setErrorChar") {
		try {
			GET_PORT;
			int val = std::stoi(argv[1]);
			port->setErrorChar(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setEofChar") {
		try {
			GET_PORT;
			int val = std::stoi(argv[1]);
			port->setEofChar(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setEvtChar") {
		try {
			GET_PORT;
			int val = std::stoi(argv[1]);
			port->setEvtChar(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "useWriteThread")
	{
		GET_PORT;
		port->useWriteThread(ans);
	}
	else if (function == "isConnected") {
		GET_PORT;
		ans << port->isConnected();
	}
	else if (function == "connect") {
		GET_PORT;
		port->connect(ans);
	}
	else if (function == "disconnect") {
		GET_PORT;
		port->disconnect(ans);
	}
	else if (function == "write") {
		GET_PORT;
		port->write(argv[1], ans);
	}
	else {
		ans << "Unrecognized function";
	}

end:
	strncpy_s(output, outputSize, ans.str().c_str(), (size_t)outputSize - 1);
	return 0;
}

//called when the extension is called with no arguments (only a function name)
void __stdcall RVExtension(char* output, int outputSize, const char* function)
{
	RVExtensionArgs(output, outputSize, function, nullptr, 0);
}