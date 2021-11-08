#pragma once
#include <iostream>
#include <cstring>
#include <sstream>
#include <string.h>
#include <mutex>
#include <atomic>
#include <map>
#include <windows.h>
#include "ReadWriteHandler.h"
#include "util.h"

static const std::string portPrefix = "\\\\.\\";

class SerialPort : public ICommunicationMethod {
private:
	//the port name as it's passed to the constructor
	std::string portNamePretty;
	//the port name as Windows sees it (prefixed by backslashes etc)
	std::string portName;

	std::string id;

	//whether we want to use the read thread
	bool usingReadThread = true;

	//whether we want to use the write thread
	bool usingWriteThread = false;

	//the file handle to the serial port
	HANDLE serialPort = nullptr;

	//DCB params
	int baudRate = 6;
	int parity = 0;
	int stopBits = 0;
	int dataBits = 4;
	bool fParity = false;
	bool fOutxCtsFlow = false;
	bool fOutxDsrFlow = false;
	int fDtrControl = 1;
	bool fDsrSensitivity = false;
	bool fTXContinueOnXoff = false;
	bool fOutX = false;
	bool fInX = false;
	bool fErrorChar = false;
	bool fNull = false;
	int fRtsControl = 1;
	bool fAbortOnError = false;
	WORD XonLim = 2048;
	WORD XoffLim = 512;
	char XonChar = 17;
	char XoffChar = 19;
	char ErrorChar = 0;
	char EofChar = 0;
	char EvtChar = 0;

	ReadWriteHandler<HANDLE>* rwHandler;

public:
	SerialPort(std::string portName, ArmaCallback callback);

	bool isConnected() {
		return serialPort != nullptr;
	}

	//sets the parameters for the serial port according to instance variables and prints errors to `out`.
	//returns false on a failed Windows API call, and true otherwise.
	bool setParams(std::stringstream& out);

	//attempts to connect to the serial port specified by instance variables, sets its parameters using `setParams`, and starts applicable read/write threads
	//prints error messages or a success message to `out`.
	void connect(std::stringstream& out);

	//attempts to disconnect from the connected serial port, and then tells the read/write threads to shut down
	//prints error/success messages to `out`
	void disconnect(std::stringstream& out);

	//attempts to start a write thread. may be called before the serial port is connected.
	//prints error/success messages to `out`.
	void useWriteThread(std::stringstream& out);

	//attempts to write `data` to the serial port, using the write thread if enabled.
	//prints error/success messages to `out`. can not detect whether a threaded write failed.
	void write(std::string data, std::stringstream& out);

	void runCommand(std::string function, std::string* argv, int argc, std::stringstream& out);
	std::string getID();

	bool isUsingWriteThread() {
		return usingWriteThread;
	}
	int getBaudRate() {
		return baudRate;
	}
	int getParity() {
		return parity;
	}
	int getStopBits() {
		return stopBits;
	}
	int getDataBits() {
		return dataBits;
	}
	bool getFParity() {
		return fParity;
	}
	bool getFOutxCtsFlow() {
		return fOutxCtsFlow;
	}
	bool getFOutxDsrFlow() {
		return fOutxDsrFlow;
	}
	int getFDtrControl() {
		return fDtrControl;
	}
	bool getFDsrSensitivity() {
		return fDsrSensitivity;
	}
	bool getFTXContinueOnXoff() {
		return fTXContinueOnXoff;
	}
	bool getFOutX() {
		return fOutX;
	}
	bool getFInX() {
		return fInX;
	}
	bool getFErrorChar() {
		return fErrorChar;
	}
	bool getFNull() {
		return fNull;
	}
	int getFRtsControl() {
		return fRtsControl;
	}
	bool getFAbortOnError() {
		return fAbortOnError;
	}
	int getXonLim() {
		return XonLim;
	}
	int getXoffLim() {
		return XoffLim;
	}
	int getXonChar() {
		return (int)XonChar;
	}
	int getXoffChar() {
		return (int)XoffChar;
	}
	int getErrorChar() {
		return (int)ErrorChar;
	}
	int getEofChar() {
		return (int)EofChar;
	}
	int getEvtChar() {
		return (int)EvtChar;
	}
	void setBaudRate(int newBaudRate, std::stringstream& out);
	void setParity(int newParity, std::stringstream& out);
	void setStopBits(int newStopBits, std::stringstream& out);
	void setDataBits(int newDataBits, std::stringstream& out);
	void setFParity(bool newFParity, std::stringstream& out);
	void setFOutxCtsFlow(bool newFOutxCtsFlow, std::stringstream& out);
	void setFOutxDsrFlow(bool newFOutxDsrFlow, std::stringstream& out);
	void setFDtrControl(int newFDtrControl, std::stringstream& out);
	void setFDsrSensitivity(bool newFDsrSensitivity, std::stringstream& out);
	void setFTXContinueOnXoff(bool newFTXContinueOnXoff, std::stringstream& out);
	void setFOutX(bool newFOutX, std::stringstream& out);
	void setFInX(bool newFInX, std::stringstream& out);
	void setFErrorChar(bool newFErrorChar, std::stringstream& out);
	void setFNull(bool newFNull, std::stringstream& out);
	void setFRtsControl(int newFRtsControl, std::stringstream& out);
	void setFAbortOnError(bool newFAbortOnError, std::stringstream& out);
	void setXonLim(WORD newXonLim, std::stringstream& out);
	void setXoffLim(WORD newXoffLim, std::stringstream& out);
	void setXonChar(char newXonChar, std::stringstream& out);
	void setXoffChar(char newXoffChar, std::stringstream& out);
	void setErrorChar(char newErrorChar, std::stringstream& out);
	void setEofChar(char newEofChar, std::stringstream& out);
	void setEvtChar(char newEvtChar, std::stringstream& out);
};

//arrays used when users select parameters for the serial port
//baud rate
static const int numRates = 14;
static const char* const baudNames[] = {
	"110",
	"300",
	"600",
	"1200",
	"2400",
	"4800",
	"9600",
	"14400",
	"19200",
	"38400",
	"57600",
	"115200",
	"128000",
	"256000"
};
static const DWORD baudRates[] = {
	CBR_110,
	CBR_300,
	CBR_600,
	CBR_1200,
	CBR_2400,
	CBR_4800,
	CBR_9600,
	CBR_14400,
	CBR_19200,
	CBR_38400,
	CBR_57600,
	CBR_115200,
	CBR_128000,
	CBR_256000
};
//parity bits
static const int numParities = 5;
static const char* const parityNames[] = {
	"No parity",
	"Odd parity",
	"Even parity",
	"Mark parity",
	"Space parity"
};
static const BYTE parities[] = {
	NOPARITY,
	ODDPARITY,
	EVENPARITY,
	MARKPARITY,
	SPACEPARITY
};
//stop bits
static const int numStopBits = 3;
static const char* const stopBitNames[] = {
	"1 stop bit",
	"1.5 stop bits",
	"2 stop bits"
};
static int stopBitValues[] = {
	ONESTOPBIT,
	ONE5STOPBITS,
	TWOSTOPBITS
};
//data bits/byte size
static const int numDataBits = 5;
static const char* const dataBitNames[] = {
	"4 data bits",
	"5 data bits",
	"6 data bits",
	"7 data bits",
	"8 data bits"
};
static const BYTE dataBitValues[] = {
	4,
	5,
	6,
	7,
	8
};