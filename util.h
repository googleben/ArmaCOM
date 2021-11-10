#pragma once

#include <cstring>
#include <sstream>
#include <string.h>
#include <windows.h>
#include "serial.h"


//takes a Windows error code (from GetLastError) and returns a string with the error message
std::string formatErr(int err);

typedef int (*ArmaCallback)(char const* name, char const* function, char const* data);

class ICommunicationMethod
{
public:
	virtual void runInstanceCommand(std::string function, std::string* argv, int argc, std::stringstream& ans, std::map<std::string, ICommunicationMethod*>& commMethds) = 0;
	virtual std::string getID() = 0;
	virtual bool isConnected() = 0;
};

enum ReadCallbackTypes {
	ON_CHAR, ON_LENGTH
};

struct ReadCallbackOptions {
	ReadCallbackTypes type;
	union { char onChar; int onLength; } value;
};

bool equalsIgnoreCase(const std::string& a, const std::string& b);