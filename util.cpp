#include "util.h"
#include "pch.h"
#include <cstring>
#include <sstream>
#include <string.h>
#include <windows.h>
#include <map>
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
	unsigned int sz = a.size();
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