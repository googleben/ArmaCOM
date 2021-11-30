#include <boost/asio.hpp>
#include "util.h"

//this file has to be separate because including boost messes with windows.h and we lose the UUID stuff
//needed in some other util functions

extern boost::asio::io_context ioContext;


int ioContextUsers = 0;
bool ioContextRunning = false;
std::thread* ioContextThread;
std::mutex ioContextMutex;
boost::asio::io_service::work* work;

void needIOContext() {
	std::unique_lock<std::mutex> lock(ioContextMutex);
	ioContextUsers++;
	if (!ioContextRunning) {
		work = new boost::asio::io_service::work(ioContext);
		ioContextThread = new std::thread([]() {
			ioContext.run();
			ioContextRunning = false;
		});
		ioContextRunning = true;
	}
}

void doneWithIOContext() {
	std::unique_lock<std::mutex> lock(ioContextMutex);
	ioContextUsers--;
	if (ioContextUsers == 0) {
		delete work;
		ioContext.stop();
		ioContextRunning = false;
	}
}