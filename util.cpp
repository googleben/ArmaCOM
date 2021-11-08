#include "util.h"
#include "pch.h"
#include <cstring>
#include <sstream>
#include <string.h>
#include <windows.h>

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