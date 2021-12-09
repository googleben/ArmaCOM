// dllmain.cpp : Defines the entry point for the DLL application.
#pragma comment( lib, "rpcrt4.lib" )
#include "boost/asio.hpp"
//TODO: add data bits
//TODO: make changes to settings affect opened ports
#include <iostream>
#include <cstring>
#include <sstream>
#include <string.h>
#include <mutex>
#include <atomic>
#include <map>
#include "serial.h"
#include "util.h"
#include "framework.h"
#include "tcpClient.h"
#include "tcpServer.h"


boost::asio::io_context ioContext;

//the callback function Arma gives us
ArmaCallback callback = nullptr;

//maps port names to SerialPorts to allow multiple simultaneous connections
std::map<std::string, ICommunicationMethod*> commMethods;

//called when the extension is loaded by ARMA
void __stdcall RVExtensionVersion(char* output, int outputSize)
{
	strncpy_s(output, outputSize, "ArmaCOM v2.0-beta", (size_t)outputSize - 1);
}

//called when the extension is loaded by ARMA
void __stdcall RVExtensionRegisterCallback(ArmaCallback c) {
	callback = c;
}

//called when the extension is called with arguments
int __stdcall RVExtensionArgs(char* output, int outputSize, const char* functionC, const char** argvC, int argc)
{
	
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
	if (equalsIgnoreCase(function, "serial")) {
		SerialPort::runStaticCommand(function, argv, argc, ans);
	}
	else if (equalsIgnoreCase(function, "tcpclient")) {
		TcpClient::runStaticCommand(function, argv, argc, ans);
	}
	else if (equalsIgnoreCase(function, "tcpserver")) {
		TcpServer::runStaticCommand(function, argv, argc, ans);
	}
	else if (equalsIgnoreCase(function, "destroy")) {
		//@GlobalCommand destroy
		//@Args instance: UUID
		//@Return Success or failure message
		//@Description Destroys an instance of a communication method if it is not currently connected
		if (argc == 0) {
			sendFailureArr(ans, "You must specify additional arguments");
			goto end;
		}
		std::string& function2 = *argv;
		if (commMethods.count(function2) == 0) {
			sendFailureArr(ans, "No such instance \"" + function2 + "\"");
			goto end;
		}
		auto commMethod = commMethods[function2];
		if (commMethod->isConnected()) {
			sendFailureArr(ans, "You must disconnect before destroying");
			goto end;
		}
		if (!commMethod->destroy()) {
			sendFailureArr(ans, "Comm method could not be destroyed. Ensure no operations are pending and the method is not connected.");
			goto end;
		}
		commMethods.erase(function2);
		delete commMethod;
		sendSuccessArr(ans, "Instance destroyed");
	}
	else {
		if (commMethods.count(function) != 0) {
			auto commMethod = commMethods[function];
			commMethod->runInstanceCommand(function, argv, argc, ans);
		}
		else {
			sendFailureArr(ans, "Unrecognized function: " + function);
		}
	}

end:
	strncpy_s(output, outputSize, ans.str().c_str(), (size_t)ans.str().length());
	return 0;
}

//called when the extension is called with no arguments (only a function name)
void __stdcall RVExtension(char* output, int outputSize, const char* function)
{
	RVExtensionArgs(output, outputSize, function, nullptr, 0);
}