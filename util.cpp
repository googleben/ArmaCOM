#include "util.h"
#include <cstring>
#include <sstream>
#include <string.h>
#include <windows.h>
#include <map>


extern ArmaCallback callback;


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

bool equalsIgnoreCase(const std::string& a, const std::string& b)
{
	//from https://stackoverflow.com/a/4119881
	size_t sz = a.size();
	if (b.size() != sz)
		return false;
	for (unsigned int i = 0; i < sz; ++i)
		if (tolower(a[i]) != tolower(b[i]))
			return false;
	return true;
}

void toLowerCase(std::string& str)
{
	for (int i = 0; i < str.length(); i++) {
		char c = str[i];
		if (c >= 'A' && c <= 'Z') {
			str[i] = c + ('a' - 'A');
		}
	}
}

std::string generateUUID() {
	UUID id;
	auto status = UuidCreateSequential(&id);
	if (status != RPC_S_OK && status != RPC_S_UUID_LOCAL_ONLY) {
		throw "Error creating UUID";
	}
	char* idCStr;
	status = UuidToStringA(&id, (RPC_CSTR*)&idCStr);
	if (status != RPC_S_OK) {
		throw "Error creating UUID";
	}
	std::string ans = idCStr;
	RpcStringFreeA((RPC_CSTR*)&idCStr);
	return ans;
}

void sendSuccessArr(std::stringstream& ans, std::string message)
{
	ans << "[\"SUCCESS\", \"" << message << "\"]";
}
void sendFailureArr(std::stringstream& ans, std::string message) {
	ans << "[\"FAILURE\", \"" << message << "\"]";
}

void callbackSuccessArr(std::string id, std::string message)
{
	auto data = ("[\"SUCCESS\", \"" + message + "\"]");
	while (callback("ArmaCOM", id.c_str(), data.c_str()) == -1) Sleep(1);
}

void callbackFailureArr(std::string id, std::string message)
{
	auto data = ("[\"FAILURE\", \"" + message + "\"]");
	while (callback("ArmaCOM", id.c_str(), data.c_str()) == -1) Sleep(1);
}

ArmaArray* ArmaArray::parse(std::string in)
{
	ArmaArray* ans = new ArmaArray();
	if (in[0] != '[') throw "Array did not start with '['";
	if (in[in.length() - 1] != ']') throw "Array did not end with ']'";
	std::vector<ArmaArray*> arrStack;
	for (size_t i = 1; i < in.length() - 1;) {
		char c = in[i];
		if (c == '[') {
			auto tmp = new ArmaArray();
			ans->add(tmp);
			arrStack.push_back(ans);
			ans = tmp;
			c = in[++i];
			while (c == ' ' || c == '\t') c = in[++i];
		}
		else if (c == ']') {
			ans = arrStack[arrStack.size() - 1];
			arrStack.pop_back();
			i++;
		}
		else if (c >= '0' && c <= '9') {
			std::string numStr;
			numStr += c;
			bool hadDecimal = false;
			i++;
			while (true) {
				if (i == in.length()) break;
				c = in[i];
				if (c == '.' && !hadDecimal) {
					numStr += c;
					hadDecimal = true;
					i++;
				}
				else if (c >= '0' && c <= '9') {
					numStr += c;
					i++;
				}
				else {
					break;
				}
			}
			double num = std::stod(numStr);
			ans->add(num);
		}
		else if (c == '"') {
			std::string str;
			i++;
			while (true) {
				if (i == in.length()) throw "Unterminated string in array";
				c = in[i];
				if (c == '"') {
					i++;
					//double quote is escaped by another double quote
					if (i != in.length() && in[i] != '"') {
						break;
					}
				}
				str += c;
				i++;
			}
			ans->add(str);
		}
		else {
			if (i <= in.length() - 4 &&
				(c == 't' || c == 'T') &&
				(in[i + 1] == 'r' || in[i + 1] == 'R') &&
				(in[i + 2] == 'u' || in[i + 2] == 'U') &&
				(in[i + 3] == 'e' || in[i + 3] == 'E')) {
				ans->add(true);
				i += 4;
			}
			else if (i <= in.length() - 5 &&
				(c == 'f' || c == 'F') &&
				(in[i + 1] == 'a' || in[i + 1] == 'A') &&
				(in[i + 2] == 'l' || in[i + 2] == 'L') &&
				(in[i + 3] == 's' || in[i + 3] == 'S') &&
				(in[i + 4] == 'e' || in[i + 4] == 'E')) {
				ans->add(false);
				i += 5;
			}
			else {
				throw "Invalid character: " + c;
			}
		}
		c = in[i];
		while (c == ' ' || c == '\t') c = in[++i];
		if (c == ',') {
			c = in[++i];
			while (c == ' ' || c == '\t') c = in[++i];
		}
		else if (c != ']') {
			throw "Invalid character: " + c;
		}
	}
	if (arrStack.size() != 0) throw "Unterminated array";
	return ans;
}

void ArmaArray::add(ArmaArray* arr) {
	ArrayElem ans;
	ans.type = ArrayElemType::ARRAY;
	ans.val.arr = arr;
	this->contents.push_back(ans);
}

void ArmaArray::add(std::string& str) {
	ArrayElem ans;
	ans.type = ArrayElemType::STRING;
	ans.val.string = new std::string(str);
	this->contents.push_back(ans);
}

inline void ArmaArray::add(double number) {
	ArrayElem ans;
	ans.type = ArrayElemType::NUMBER;
	ans.val.number = number;
	this->contents.push_back(ans);
}

inline void ArmaArray::add(bool boolean) {
	ArrayElem ans;
	ans.type = ArrayElemType::BOOLEAN;
	ans.val.boolean = boolean;
	this->contents.push_back(ans);
}

void ArmaArray::appendAsString(std::stringstream& ans)
{
	ans << "[";
	auto start = this->contents.begin();
	auto it = start;
	auto end = this->contents.end();
	for (; it < end; it++) {
		if (it != start) {
			ans << ", ";
		}
		auto elem = *it;
		if (elem.type == ArrayElemType::ARRAY) {
			elem.val.arr->appendAsString(ans);
		}
		else if (elem.type == ArrayElemType::STRING) {
			ans << "\"";
			ans << elem.val.string;
			ans << "\"";
		}
		else if (elem.type == ArrayElemType::NUMBER) {
			ans << elem.val.number;
		}
		else if (elem.type == ArrayElemType::BOOLEAN) {
			if (elem.val.boolean) {
				ans << "true";
			}
			else {
				ans << "false";
			}
		}
	}
	ans << "]";
}

std::string ArmaArray::toString()
{
	std::stringstream ans;
	this->appendAsString(ans);
	return ans.str();
}

ArmaArray::~ArmaArray()
{
	auto it = this->contents.begin();
	auto end = this->contents.end();
	for (; it < end; it++) {
		auto elem = *it;
		if (elem.type == ArrayElemType::ARRAY) {
			delete elem.val.arr;
		}
		else if (elem.type == ArrayElemType::STRING) {
			delete elem.val.string;
		}
	}
}

bool operator==(const ReadCallbackOptions& a, const ReadCallbackOptions& b) {
	if (a.type != b.type) {
		return false;
	}
	if (a.type == ReadCallbackTypes::ON_CHAR) {
		return a.value.onChar == b.value.onChar;
	}
	else if (a.type == ReadCallbackTypes::ON_LENGTH) {
		return a.value.onLength == b.value.onLength;
	}
	else {
		throw "Unimplemented equality case for ReadCallbackOptions";
	}
}

bool operator!=(const ReadCallbackOptions& a, const ReadCallbackOptions& b) {
	return !(a == b);
}