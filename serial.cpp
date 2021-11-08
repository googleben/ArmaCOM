#include "pch.h"
#include "serial.h"
#include "util.h"
#include <functional>

SerialPort::SerialPort(std::string portName, ArmaCallback callback) {
	UUID id;
	UuidCreate(&id);
	char* idCStr;
	UuidToStringA(&id, (RPC_CSTR*)&idCStr);
	this->id = idCStr;
	RpcStringFreeA((RPC_CSTR*)&idCStr);
	this->portName = portPrefix + portName;
	this->portNamePretty = portName;
	std::function<bool(HANDLE*, char*, DWORD, DWORD*)> writeFunc = [](HANDLE* handle, char* data, DWORD dataSize, DWORD* written) {
		return (bool)WriteFile(*handle, data, dataSize, written, nullptr);
	};
	std::function<bool(HANDLE*, char*)> readFunc = [](HANDLE* handle, char* readChar) {
		DWORD charsRead;
		auto ok = ReadFile(*handle, readChar, 1, &charsRead, nullptr);
		return ok && charsRead == 1;
	};
	this->rwHandler = new ReadWriteHandler<HANDLE>(&(this->serialPort), this, writeFunc, readFunc, callback, true, false);
}


//sets the parameters for the serial port according to instance variables and prints errors to `out`.
//returns false on a failed Windows API call, and true otherwise.

bool SerialPort::setParams(std::stringstream& out) {
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

void SerialPort::connect(std::stringstream& out) {
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

	this->rwHandler->startThreads();

	out << "Succesfully connected to serial port";
}

//attempts to disconnect from the connected serial port, and then tells the read/write threads to shut down
//prints error/success messages to `out`

void SerialPort::disconnect(std::stringstream& out) {
	this->rwHandler->stopThreads();
	if (!CloseHandle(serialPort)) {
		DWORD err = GetLastError();
		out << "Error while disconnecting: " << formatErr(err);
	}
	else {
		out << "Disconnected successfully";
	}
	serialPort = nullptr;
}

//attempts to start a write thread. may be called before the serial port is connected.
//prints error/success messages to `out`.

void SerialPort::useWriteThread(std::stringstream& out) {
	this->rwHandler->enableWriteThread(out);
}

//attempts to write `data` to the serial port, using the write thread if enabled.
//prints error/success messages to `out`. can not detect whether a threaded write failed.

void SerialPort::write(std::string data, std::stringstream& out) {
	this->rwHandler->write(data, out);
}

void SerialPort::setBaudRate(int newBaudRate, std::stringstream& out) {
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

void SerialPort::setParity(int newParity, std::stringstream& out) {
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

void SerialPort::setStopBits(int newStopBits, std::stringstream& out) {
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

void SerialPort::setDataBits(int newDataBits, std::stringstream& out) {
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

void SerialPort::setFParity(bool newFParity, std::stringstream& out) {
	fParity = newFParity;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setFOutxCtsFlow(bool newFOutxCtsFlow, std::stringstream& out) {
	fOutxCtsFlow = newFOutxCtsFlow;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setFOutxDsrFlow(bool newFOutxDsrFlow, std::stringstream& out) {
	fOutxDsrFlow = newFOutxDsrFlow;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setFDtrControl(int newFDtrControl, std::stringstream& out) {
	fDtrControl = newFDtrControl;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setFDsrSensitivity(bool newFDsrSensitivity, std::stringstream& out) {
	fDsrSensitivity = newFDsrSensitivity;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setFTXContinueOnXoff(bool newFTXContinueOnXoff, std::stringstream& out) {
	fTXContinueOnXoff = newFTXContinueOnXoff;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setFOutX(bool newFOutX, std::stringstream& out) {
	fOutX = newFOutX;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setFInX(bool newFInX, std::stringstream& out) {
	fInX = newFInX;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setFErrorChar(bool newFErrorChar, std::stringstream& out) {
	fErrorChar = newFErrorChar;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setFNull(bool newFNull, std::stringstream& out) {
	fNull = newFNull;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setFRtsControl(int newFRtsControl, std::stringstream& out) {
	fRtsControl = newFRtsControl;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setFAbortOnError(bool newFAbortOnError, std::stringstream& out) {
	fAbortOnError = newFAbortOnError;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setXonLim(WORD newXonLim, std::stringstream& out) {
	XonLim = newXonLim;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setXoffLim(WORD newXoffLim, std::stringstream& out) {
	XoffLim = newXoffLim;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setXonChar(char newXonChar, std::stringstream& out) {
	XonChar = newXonChar;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setXoffChar(char newXoffChar, std::stringstream& out) {
	XoffChar = newXoffChar;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setErrorChar(char newErrorChar, std::stringstream& out) {
	ErrorChar = newErrorChar;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setEofChar(char newEofChar, std::stringstream& out) {
	EofChar = newEofChar;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::setEvtChar(char newEvtChar, std::stringstream& out) {
	EvtChar = newEvtChar;
	if (isConnected()) {
		setParams(out);
	}
}

void SerialPort::runCommand(std::string myId, std::string* argv, int argc, std::stringstream& ans) {
	std::string& function = argv[0];
	if (function == "getBaudRate") {
		ans << this->getBaudRate();
	}
	else if (function == "getParity") {
		ans << this->getParity();
	}
	else if (function == "getStopBits") {
		ans << this->getStopBits();
	}
	else if (function == "getDataBits") {
		ans << this->getDataBits();
	}
	else if (function == "getFParity") {
		ans << this->getFParity();
	}
	else if (function == "getFOutxCtsFlow") {
		ans << this->getFOutxCtsFlow();
	}
	else if (function == "getFOutxDsrFlow") {
		ans << this->getFOutxDsrFlow();
	}
	else if (function == "getFDtrControl") {
		ans << this->getFDtrControl();
	}
	else if (function == "getFDsrSensitivity") {
		ans << this->getFDsrSensitivity();
	}
	else if (function == "getFTXContinueOnXoff") {
		ans << this->getFTXContinueOnXoff();
	}
	else if (function == "getFOutX") {
		ans << this->getFOutX();
	}
	else if (function == "getFInX") {
		ans << this->getFInX();
	}
	else if (function == "getFErrorChar") {
		ans << this->getFErrorChar();
	}
	else if (function == "getFNull") {
		ans << this->getFNull();
	}
	else if (function == "getFRtsControl") {
		ans << this->getFRtsControl();
	}
	else if (function == "getFAbortOnError") {
		ans << this->getFAbortOnError();
	}
	else if (function == "getXonLim") {
		ans << this->getXonLim();
	}
	else if (function == "getXoffLim") {
		ans << this->getXoffLim();
	}
	else if (function == "getXonChar") {
		ans << this->getXonChar();
	}
	else if (function == "getXoffChar") {
		ans << this->getXoffChar();
	}
	else if (function == "getErrorChar") {
		ans << this->getErrorChar();
	}
	else if (function == "getEofChar") {
		ans << this->getEofChar();
	}
	else if (function == "getEvtChar") {
		ans << this->getEvtChar();
	}
	else if (function == "isUsingWriteThread") {
		ans << this->isUsingWriteThread();
	}
	else if (function == "setBaudRate") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[1]);
			this->setBaudRate(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setParity") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[1]);
			this->setParity(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setStopBits") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[1]);
			this->setStopBits(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setDataBits") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[1]);
			this->setDataBits(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setFParity") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFParity(val, ans);
	}
	else if (function == "setFOutxCtsFlow") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFOutxCtsFlow(val, ans);
	}
	else if (function == "setFOutxDsrFlow") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFOutxDsrFlow(val, ans);
	}
	else if (function == "setFDtrControl") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		int val;
		if (argv[1] == "0") val = 0;
		else if (argv[1] == "1") val = 1;
		else if (argv[1] == "2") val = 2;
		else {
			ans << "Expected 0, 1, or 2";
			goto end;
		}
		this->setFDtrControl(val, ans);
	}
	else if (function == "setFDsrSensitivity") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFDsrSensitivity(val, ans);
	}
	else if (function == "setFTXContinueOnXoff") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFTXContinueOnXoff(val, ans);
	}
	else if (function == "setFOutX") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFOutX(val, ans);
	}
	else if (function == "setFInX") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFInX(val, ans);
	}
	else if (function == "setFErrorChar") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFErrorChar(val, ans);
	}
	else if (function == "setFNull") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFNull(val, ans);
	}
	else if (function == "setFRtsControl") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		int val;
		if (argv[1] == "0") val = 0;
		else if (argv[1] == "1") val = 1;
		else if (argv[1] == "2") val = 2;
		else if (argv[1] == "3") val = 3;
		else {
			ans << "Expected 0, 1, 2, or 3";
			goto end;
		}
		this->setFRtsControl(val, ans);
	}
	else if (function == "setFAbortOnError") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[1] == "true" || argv[1] == "1") val = true;
		else if (argv[1] == "false" || argv[1] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFAbortOnError(val, ans);
	}
	else if (function == "setXonLim") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[1]);
			this->setXonLim(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setXoffLim") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[1]);
			this->setXoffLim(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setXonChar") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[1]);
			this->setXonChar(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setXoffChar") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[1]);
			this->setXoffChar(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setErrorChar") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[1]);
			this->setErrorChar(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setEofChar") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[1]);
			this->setEofChar(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "setEvtChar") {
		if (argc == 1) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[1]);
			this->setEvtChar(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "useWriteThread") {
		this->useWriteThread(ans);
	}
	else if (function == "callbackOnChar") {
		if (argc == 1 || argv[1].length() == 0) {
			ans << "Additional argument required";
			goto end;
		}
		this->rwHandler->callbackOptions.type = ReadCallbackTypes::ON_CHAR;
		this->rwHandler->callbackOptions.value.onChar = argv[1][0];
	}
	else if (function == "callbackOnCharCode") {
		if (argc == 1 || argv[1].length() == 0) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[1]);
			this->rwHandler->callbackOptions.type = ReadCallbackTypes::ON_CHAR;
			this->rwHandler->callbackOptions.value.onChar = (char)val;
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "callbackOnLength") {
		if (argc == 1 || argv[1].length() == 0) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[1]);
			if (val < 1) {
				ans << "Length must be at least 1";
				goto end;
			}
			this->rwHandler->callbackOptions.type = ReadCallbackTypes::ON_LENGTH;
			this->rwHandler->callbackOptions.value.onLength = val;
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (function == "isConnected") {
		ans << this->isConnected();
	}
	else if (function == "connect") {
		this->connect(ans);
	}
	else if (function == "disconnect") {
		this->disconnect(ans);
	}
	else if (function == "write") {
		this->write(argv[1], ans);
	}
	else {
		ans << "Unrecognized function \"" << function << "\" on serial port";
	}
end:
	return;
}

std::string SerialPort::getID()
{
	return this->id;
}
