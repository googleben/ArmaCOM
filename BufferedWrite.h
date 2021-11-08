#pragma once
#include "util.h"
#include <string.h>
#include <functional>
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