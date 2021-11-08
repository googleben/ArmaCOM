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
#include "serial.h"
#include "util.h"
#include "framework.h"

	

//the callback function Arma gives us
static ArmaCallback callback = nullptr;






static std::map<std::string, SerialPort*> serialPorts;

//maps port names to SerialPorts to allow multiple simultaneous connections
static std::map<std::string, ICommunicationMethod*> commMethods;

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
	if (function == "serial") {
		if (argc == 0) {
			ans << "You must specify additional arguments";
			goto end;
		}
		std::string& function2 = *argv;
		if (function2 == "create") {
			SerialPort* port;
			if (argc == 1) { ans << "You must specify a port for this command"; goto end; }
			if (QueryDosDeviceA(argv[1].c_str(), pathBuffer, 800) == 0) {
					ans << "The specified port (\"" << argv[1] << "\") is invalid";
					goto end;
			}
			if (serialPorts.count(argv[1]) == 0) {
					port = new SerialPort(argv[1], callback);
					serialPorts[argv[1]] = port;
					commMethods[port->getID()] = port;
			}
			else {
				port = serialPorts[argv[1]];
			}
			ans << port->getID();
		}
		else if (function2 == "listPorts") {
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
		else if (function2 == "listBaudRates") {
			for (int i = 0; i < numRates; i++) {
				if (i != 0) ans << ", ";
				ans << i << ": " << baudNames[i];
			}
		}
		else if (function2 == "listStopBits") {
			for (int i = 0; i < numStopBits; i++) {
				if (i != 0) ans << ", ";
				ans << i << ": " << stopBitNames[i];
			}
		}
		else if (function2 == "listParities") {
			for (int i = 0; i < numParities; i++) {
				if (i != 0) ans << ", ";
				ans << i << ": " << parityNames[i];
			}
		}
		else if (function2 == "listDataBits") {
			for (int i = 0; i < numDataBits; i++) {
				if (i != 0) ans << ", ";
				ans << i << ": " << dataBitNames[i];
			}
		}
	}
	else {
		if (commMethods.count(function) != 0) {
			auto commMethod = commMethods[function];
			commMethod->runCommand(function, argv, argc, ans);
		}
		else {
			ans << "Unrecognized function";
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