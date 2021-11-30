#pragma once

#include <cstring>
#include <sstream>
#include <string.h>
#include <map>
#include <cstdarg>
#include <vector>


//takes a Windows error code (from GetLastError) and returns a string with the error message
std::string formatErr(int err);

typedef int (*ArmaCallback)(char const* name, char const* function, char const* data);

class ICommunicationMethod
{
public:
	virtual void runInstanceCommand(std::string function, std::string* argv, int argc, std::stringstream& ans) = 0;
	virtual std::string getID() = 0;
	virtual bool isConnected() = 0;
	virtual bool destroy() = 0;
};

class ArmaArray {
private:
	enum ArrayElemType {
		ARRAY, STRING, NUMBER, BOOLEAN
	};
	struct ArrayElem {
		ArrayElemType type;
		union {
			ArmaArray* arr;
			std::string* string;
			double number;
			bool boolean;
		} val;
	};
	std::vector<ArrayElem> contents;
public:
	ArmaArray() {}
	static ArmaArray* parse(std::string in);
	void add(ArmaArray* arr);
	void add(std::string& str);
	void add(double number);
	void add(bool boolean);
	void appendAsString(std::stringstream& ans);
	std::string toString();
	~ArmaArray();
};

enum ReadCallbackTypes {
	ON_CHAR, ON_LENGTH
};

struct ReadCallbackOptions {
	ReadCallbackTypes type;
	union { char onChar; int onLength; } value;
};
bool operator==(const ReadCallbackOptions& a, const ReadCallbackOptions& b);
bool operator!=(const ReadCallbackOptions& a, const ReadCallbackOptions& b);

bool equalsIgnoreCase(const std::string& a, const std::string& b);
std::string generateUUID();

void sendSuccessArr(std::stringstream& ans, std::string message);
void sendFailureArr(std::stringstream& ans, std::string message);

void callbackSuccessArr(std::string id, std::string message);
void callbackFailureArr(std::string id, std::string message);

void needIOContext();

void doneWithIOContext();