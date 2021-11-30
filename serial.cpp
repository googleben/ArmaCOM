#include "serial.h"
#include "util.h"
#include <functional>

//@CommMethod Serial
//@Description This communication method is an interface for serial/COM ports. Serial ports are different from most other (read: modern) methods of communicating with computers.
//@Description The main reason this is included is for use with small board computers like Arduinos and Raspberry Pis, but may be useful for other, typically legacy, hardware.
//@Description Users of legacy or uncommon hardware should beware that serial communication is finnicky even in the most favorable conditions, and this has only been tested thus far with modern equipment.
//@Description An important note is that due to the limitations of the technology, only **one** instance of this communication method may exist per COM port.
//@Description If you attempt to create a second instance for a serial port that already has an instance, the extension will simply return the existing instance.
//@Description This functionality may be useful since you can regain a port's UUID if you lost it by attempting to create a new instance, but is more likely to be a source of headaches.
//@Description The extension exposes a large number of low-level parameters through getters and setters in the hope that it will be useful to users with nonstandard or legacy hardware.
//@Description If you're using modern "happy" hardware like an Arduino, or don't know what any of it means, there's a very good chance the defaults will work fine.
//@Description The only parameters most users will need to touch are baud rate, whether writes are threaded, and when the extension sends data back to Arma.
//@Description Threaded writes are off by default just because I haven't tested their stability thoroughly, but you should turn threaded writes on unless you have issues.
//@Description Despite modern hardware being incredibly fast, serial communications defaults to the snail-like 9600 baud, and even at high baud rates the overhead of writes puts a lower bound of rougly **20ms for any write operation on my machine, even using emulated serial ports**.
//@Description Of course threaded writes don't make that latency magically disappear, but they can shunt it off to one of the 3+ cores Arma never uses and let Arma get on with whatever stuff it needs to do in the meantime.
//@Description Because Bohemia is working with a very old codebase that was absolutely not written with maintainability and performance in mind, Arma is single-threaded. SQF manages to fake the ability to run multiple scripts at once by using a basic scheduler that begins execution of a "thread", runs it until it passes some predefined allotment of time, and then moves on to the next "thread" if it isn't terribly behind and needs to yield its CPU time to other things.
//@Description This tends to work well enough with how fast modern CPUs are, but a very important limitation is that the SQF VM **cannot make an extension yield its time**. When `callExtension` is run, Arma has to stop pretty much everything it's doing until the extension returns, which makes waiting around for a write to finish for 20+ms a very bad idea.
//@Description If you're lucky enough for your game to be running at 60 FPS, that means a new frame comes through every 16.6ms - that's right, a single write to a serial port takes longer than running physics simulations and rendering an entire frame by almost 25%.
//@Description So please, turn on threaded writes, and if that's not good enough, consider something other than the terribly dated technology that is serial communications.

//since only 1 connection to a serial port can be active at once usually,
//we cache them here to make life a little easier for devs
static std::map<std::string, SerialPort*> serialPorts;

extern std::map<std::string, ICommunicationMethod*> commMethods;
extern ArmaCallback callback;

SerialPort::SerialPort(std::string portName) {
	this->id = generateUUID();
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
	this->rwHandler = new ReadWriteHandler<HANDLE>(&(this->serialPort), this, writeFunc, readFunc, true, false);
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
	if (serialPort != nullptr || this->connected) {
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

void SerialPort::enableWriteThread(std::stringstream& out) {
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

bool SerialPort::destroy() {
	if (this->connected) return false;
	serialPorts.erase(this->id);
	return true;
}

void SerialPort::runInstanceCommand(std::string myId, std::string* argv, int argc, std::stringstream& ans) {
	std::string& function = argv[0];
	argv++;
	argc--;
	if (equalsIgnoreCase(function, "getBaudRate")) {
		//@InstanceCommand serial.getBaudRate
		//@Args 
		//@Return The index of the currently set baud rate
		//@Description Gets the index of the set baud rate, not the actual baud rate
		ans << this->getBaudRate();
	}
	else if (equalsIgnoreCase(function, "getParity")) {
		//@InstanceCommand serial.getParity
		//@Args 
		//@Return The index of the currently set parity bit(s)
		//@Description Gets the index of the set parity bit(s), not the actual parity bit(s)
		ans << this->getParity();
	}
	else if (equalsIgnoreCase(function, "getStopBits")) {
		//@InstanceCommand serial.getStopBits
		//@Args 
		//@Return The index of the currently set stop bit(s)
		//@Description Gets the index of the set stop bit(s), not the actual stop bit(s)
		ans << this->getStopBits();
	}
	else if (equalsIgnoreCase(function, "getDataBits")) {
		//@InstanceCommand serial.getDataBits
		//@Args 
		//@Return The index of the currently set data bits
		//@Description Gets the index of the set data bits, not the actual data bits
		ans << this->getDataBits();
	}
	else if (equalsIgnoreCase(function, "getFParity")) {
		//@InstanceCommand serial.getFParity
		//@Args 
		//@Return `true` or `false` based on the currently set `fParity`
		//@Description If `true`, parity checking is performed. Default: `false`.
		ans << this->getFParity();
	}
	else if (equalsIgnoreCase(function, "getFOutxCtsFlow")) {
		//@InstanceCommand serial.getFOutxCtsFlow
		//@Args 
		//@Return `true` or `false` based on the currently set `fOutxCtsFlow`
		//@Description If `true`, the Clear-To-Send signal is monitored, and when the CTS is turned off, output is suspended until the CTS is sent again. Default: `false`.
		ans << this->getFOutxCtsFlow();
	}
	else if (equalsIgnoreCase(function, "getFOutxDsrFlow")) {
		//@InstanceCommand serial.getFOutxDsrFlow
		//@Args 
		//@Return `true` or `false` based on the currently set `fOutxDsrFlow`
		//@Description If `true`, the Data-Set-Ready signal is monitored, and when the DSR is turned off, output is suspended until the DSR is sent again. Default: `false`.
		ans << this->getFOutxDsrFlow();
	}
	else if (equalsIgnoreCase(function, "getFDtrControl")) {
		//@InstanceCommand serial.getFDtrControl
		//@Args 
		//@Return `0`, `1`, or `2` based on the currently set `fDtrControl`
		//@Description If `0`, the Data-Terminal-Ready line is disabled. If `1`, the DTR line is enabled and left on. If `2`, it is an error to adjust the DTR line. Default: `1`.
		ans << this->getFDtrControl();
	}
	else if (equalsIgnoreCase(function, "getFDsrSensitivity")) {
		//@InstanceCommand serial.getFDsrSensitivity
		//@Args 
		//@Return `true` or `false` based on the currently set `fDsrSensitivity`
		//@Description If `true`, the driver is sensitive to the state of the DSR signal - all recieved bytes will be ignored unless the DSR input line is high. Default: `false`.
		ans << this->getFDsrSensitivity();
	}
	else if (equalsIgnoreCase(function, "getFTXContinueOnXoff")) {
		//@InstanceCommand serial.getFTXContinueOnXoff
		//@Args 
		//@Return `true` or `false` based on the currently set `fTXContinueOnXoff`
		//@Description If `true`, transmission continues after the input buffer has come within `XoffLim` bytes of being full and the driver has transmitted the `XoffChar` character to stop receiving bytes. If `false`, transmission does not continue until the input buffer is within `XonLim` bytes of being empty and the driver has transmitted the `XonChar` character to resume reception. Default: `false`.
		ans << this->getFTXContinueOnXoff();
	}
	else if (equalsIgnoreCase(function, "getFOutX")) {
		//@InstanceCommand serial.getFOutX
		//@Args 
		//@Return `true` or `false` based on the currently set `fOutX`
		//@Description If `true`, transmission stops when the `XoffChar` character is received and starts again when the `XonChar` character is received. Default: `false`.
		ans << this->getFOutX();
	}
	else if (equalsIgnoreCase(function, "getFInX")) {
		//@InstanceCommand serial.getFInX
		//@Args 
		//@Return `true` or `false` based on the currently set `fInX`
		//@Description If `true`, the `XoffChar` character is sent when the input buffer comes within `XoffLim` bytes of being full, and the `XonChar` character is sent when the input buffer comes within `XonLim` bytes of being empty. Defualt: `false`.
		ans << this->getFInX();
	}
	else if (equalsIgnoreCase(function, "getFErrorChar")) {
		//@InstanceCommand serial.getFErrorChar
		//@Args 
		//@Return `true` or `false` based on the currently set `fErrorChar`
		//@Description If `true` and `fParity` is true, bytes received with parity errors are replaced with `ErrorChar`. Defualt: `false`.
		ans << this->getFErrorChar();
	}
	else if (equalsIgnoreCase(function, "getFNull")) {
		//@InstanceCommand serial.getFNull
		//@Args 
		//@Return `true` or `false` based on the currently set `fNull`
		//@Description If `true`, null bytes are discarded when received. Defualt: `false`.
		ans << this->getFNull();
	}
	else if (equalsIgnoreCase(function, "getFRtsControl")) {
		//@InstanceCommand serial.getFRtsControl
		//@Args 
		//@Return `0`, `1`, `2`, or `3` based on the currently set `fRtsControl`
		//@Description If `0`, the Request-To-Send line is disabled. If `1`, the RTS line is opened and left on. If `2`, RTS handshaking is enabled - the drived raises the RTS line when the input buffer is less than one half full and lowers the RTS line when the buffer is more than three quarters full, and it is an error to adjust the RTS line. If `3`, the RTS line will be high if bytes are available for transmission, and after all buffered bytes have been sent, the RTS line will be low. Default: `1`.
		ans << this->getFRtsControl();
	}
	else if (equalsIgnoreCase(function, "getFAbortOnError")) {
		//@InstanceCommand serial.getFAbortOnError
		//@Args 
		//@Return `true` or `false` based on the currently set `fAbortOnError`
		//@Description If `true`, the driver terminates all read and write operations with an error status if an error occurs. The driver will not accept any further communications until the extension clears the error (currently not implemented). Default: `false`.
		ans << this->getFAbortOnError();
	}
	else if (equalsIgnoreCase(function, "getXonLim")) {
		//@InstanceCommand serial.getXonLim
		//@Args 
		//@Return The current XonLim
		//@Description See `fInX`, `fRtsControl`, and `fDtrControl`. Default: `2048`.
		ans << this->getXonLim();
	}
	else if (equalsIgnoreCase(function, "getXoffLim")) {
		//@InstanceCommand serial.getXoffLim
		//@Args 
		//@Return The current XoffChar
		//@Description See `fInX`, `fRtsControl`, and `fDtrControl`. Default: `512`.
		ans << this->getXoffLim();
	}
	else if (equalsIgnoreCase(function, "getXonChar")) {
		//@InstanceCommand serial.getXonChar
		//@Args 
		//@Return The current ASCII charcode of XonChar
		//@Description Default: `17` (device control 1).
		ans << this->getXonChar();
	}
	else if (equalsIgnoreCase(function, "getXoffChar")) {
		//@InstanceCommand serial.getXoffChar
		//@Args 
		//@Return The current ASCII charcode of XoffChar
		//@Description Default: `19` (device control 3).
		ans << this->getXoffChar();
	}
	else if (equalsIgnoreCase(function, "getErrorChar")) {
		//@InstanceCommand serial.getErrorChar
		//@Args 
		//@Return The current ASCII charcode of errorChar
		//@Description Default: `0`. 
		ans << this->getErrorChar();
	}
	else if (equalsIgnoreCase(function, "getEofChar")) {
		//@InstanceCommand serial.getEofChar
		//@Args 
		//@Return The current ASCII charcode of eofChar
		//@Description The character used to signal the end of data. Default: `0`.
		ans << this->getEofChar();
	}
	else if (equalsIgnoreCase(function, "getEvtChar")) {
		//@InstanceCommand serial.getEvtChar
		//@Args 
		//@Return The current ASCII charcode of evtChar
		//@Description he character used to signal an event. Default: `0`.
		ans << this->getEvtChar();
	}
	else if (equalsIgnoreCase(function, "isUsingWriteThread")) {
		//@InstanceCommand serial.isUsingWriteThread
		//@Args 
		//@Return `true` or `false` based on whether this serial port is using a thread for writes
		//@Description May dramatically improve performance. See the main header of this communication method for more information.
		ans << this->isUsingWriteThread();
	}
	else if (equalsIgnoreCase(function, "setBaudRate")) {
		//@InstanceCommand serial.setBaudRate
		//@Args baudRateIndex: int
		//@Return 
		//@Description Sets the baud rate. `baudRateIndex` is the index of the desired baud rate (from `listBaudRates`).
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[0]);
			this->setBaudRate(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (equalsIgnoreCase(function, "setParity")) {
		//@InstanceCommand serial.setParity
		//@Args parityIndex: int
		//@Return 
		//@Description Sets the parity. `parityIndex` is the index of the desired parity (from `listParities`).
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[0]);
			this->setParity(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (equalsIgnoreCase(function, "setStopBits")) {
		//@InstanceCommand serial.setStopBits
		//@Args stopBitsIndex: int
		//@Return 
		//@Description Sets the stop bit(s). `stopBitsIndex` is the index of the desired stop bit(s) (from `listStopBits`).
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[0]);
			this->setStopBits(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (equalsIgnoreCase(function, "setDataBits")) {
		//@InstanceCommand serial.setDataBits
		//@Args dataBitsIndex: int
		//@Return 
		//@Description Sets the data bits. `dataBitsIndex` is the index of the desired baud rate (from `listDataBits`).
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[0]);
			this->setDataBits(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (equalsIgnoreCase(function, "setFParity")) {
		//@InstanceCommand serial.setFParity
		//@Args fParity: bool
		//@Return 
		//@Description Sets `fParity`. If `true`, parity checking is performed. Default: `false`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[0] == "true" || argv[0] == "1") val = true;
		else if (argv[0] == "false" || argv[0] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFParity(val, ans);
	}
	else if (equalsIgnoreCase(function, "setFOutxCtsFlow")) {
		//@InstanceCommand serial.setFOutxCtsFlow
		//@Args fOutxCtsFlow: bool
		//@Return 
		//@Description Sets `fOutxCtsFlow`. If `true`, the Clear-To-Send signal is monitored, and when the CTS is turned off, output is suspended until the CTS is sent again. Default: `false`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[0] == "true" || argv[0] == "1") val = true;
		else if (argv[0] == "false" || argv[0] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFOutxCtsFlow(val, ans);
	}
	else if (equalsIgnoreCase(function, "setFOutxDsrFlow")) {
		//@InstanceCommand serial.setFOutxDsrFlow
		//@Args fOutxDsrFlow: bool
		//@Return 
		//@Description Sets fOutxDsrFlow. If `true`, the Data-Set-Ready signal is monitored, and when the DSR is turned off, output is suspended until the DSR is sent again. Default: `false`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[0] == "true" || argv[0] == "1") val = true;
		else if (argv[0] == "false" || argv[0] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFOutxDsrFlow(val, ans);
	}
	else if (equalsIgnoreCase(function, "setFDtrControl")) {
		//@InstanceCommand serial.setFDtrControl
		//@Args fDtsControl: 0 | 1 | 2
		//@Return 
		//@Description Sets fDtrControl. If `0`, the Data-Terminal-Ready line is disabled. If `1`, the DTR line is enabled and left on. If `2`, it is an error to adjust the DTR line. Default: `1`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		int val;
		if (argv[0] == "0") val = 0;
		else if (argv[0] == "1") val = 1;
		else if (argv[0] == "2") val = 2;
		else {
			ans << "Expected 0, 1, or 2";
			goto end;
		}
		this->setFDtrControl(val, ans);
	}
	else if (equalsIgnoreCase(function, "setFDsrSensitivity")) {
		//@InstanceCommand serial.setFDsrSensitivity
		//@Args fDsrSensitivity: bool
		//@Return 
		//@Description Sets fDsrSensitivity. If `true`, the driver is sensitive to the state of the DSR signal - all recieved bytes will be ignored unless the DSR input line is high. Default: `false`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[0] == "true" || argv[0] == "1") val = true;
		else if (argv[0] == "false" || argv[0] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFDsrSensitivity(val, ans);
	}
	else if (equalsIgnoreCase(function, "setFTXContinueOnXoff")) {
		//@InstanceCommand serial.setFTXContinueOnXoff
		//@Args fTXContinueOnXoff: bool
		//@Return 
		//@Description Sets fTXContinueOnXoff. If `true`, transmission continues after the input buffer has come within `XoffLim` bytes of being full and the driver has transmitted the `XoffChar` character to stop receiving bytes. If `false`, transmission does not continue until the input buffer is within `XonLim` bytes of being empty and the driver has transmitted the `XonChar` character to resume reception. Default: `false`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[0] == "true" || argv[0] == "1") val = true;
		else if (argv[0] == "false" || argv[0] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFTXContinueOnXoff(val, ans);
	}
	else if (equalsIgnoreCase(function, "setFOutX")) {
		//@InstanceCommand serial.setFOutX
		//@Args fOutX: bool
		//@Return 
		//@Description Sets fOutX. If `true`, transmission stops when the `XoffChar` character is received and starts again when the `XonChar` character is received. Default: `false`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[0] == "true" || argv[0] == "1") val = true;
		else if (argv[0] == "false" || argv[0] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFOutX(val, ans);
	}
	else if (equalsIgnoreCase(function, "setFInX")) {
		//@InstanceCommand serial.setFInX
		//@Args fInX: bool
		//@Return 
		//@Description Sets fInX. If `true`, the `XoffChar` character is sent when the input buffer comes within `XoffLim` bytes of being full, and the `XonChar` character is sent when the input buffer comes within `XonLim` bytes of being empty. Defualt: `false`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[0] == "true" || argv[0] == "1") val = true;
		else if (argv[0] == "false" || argv[0] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFInX(val, ans);
	}
	else if (equalsIgnoreCase(function, "setFErrorChar")) {
		//@InstanceCommand serial.setFErrorChar
		//@Args fErrorChar: bool
		//@Return 
		//@Description Sets fErrorChar. If `true` and `fParity` is true, bytes received with parity errors are replaced with `ErrorChar`. Defualt: `false`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[0] == "true" || argv[0] == "1") val = true;
		else if (argv[0] == "false" || argv[0] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFErrorChar(val, ans);
	}
	else if (equalsIgnoreCase(function, "setFNull")) {
		//@InstanceCommand serial.setFNull
		//@Args fNull: bool
		//@Return 
		//@Description Sets fNull. If `true`, null bytes are discarded when received. Defualt: `false.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[0] == "true" || argv[0] == "1") val = true;
		else if (argv[0] == "false" || argv[0] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFNull(val, ans);
	}
	else if (equalsIgnoreCase(function, "setFRtsControl")) {
		//@InstanceCommand serial.setFRtsControl
		//@Args fRtsControl: 0 | 1 | 2 | 3
		//@Return 
		//@Description Sets fRtsControl. If `0`, the Request-To-Send line is disabled. If `1`, the RTS line is opened and left on. If `2`, RTS handshaking is enabled - the drived raises the RTS line when the input buffer is less than one half full and lowers the RTS line when the buffer is more than three quarters full, and it is an error to adjust the RTS line. If `3`, the RTS line will be high if bytes are available for transmission, and after all buffered bytes have been sent, the RTS line will be low. Default: `1`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		int val;
		if (argv[0] == "0") val = 0;
		else if (argv[0] == "1") val = 1;
		else if (argv[0] == "2") val = 2;
		else if (argv[0] == "3") val = 3;
		else {
			ans << "Expected 0, 1, 2, or 3";
			goto end;
		}
		this->setFRtsControl(val, ans);
	}
	else if (equalsIgnoreCase(function, "setFAbortOnError")) {
		//@InstanceCommand serial.setFAbortOnError
		//@Args fAbortOnError: bool
		//@Return 
		//@Description Sets fAbortOnError. If `true`, the driver terminates all read and write operations with an error status if an error occurs. The driver will not accept any further communications until the extension clears the error (currently not implemented). Default: `false`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		bool val;
		if (argv[0] == "true" || argv[0] == "1") val = true;
		else if (argv[0] == "false" || argv[0] == "0") val = false;
		else {
			ans << "Expected true, false, 1, or 0";
			goto end;
		}
		this->setFAbortOnError(val, ans);
	}
	else if (equalsIgnoreCase(function, "setXonLim")) {
		//@InstanceCommand serial.setXonLim
		//@Args XonLim: int
		//@Return 
		//@Description Sets XonLim. See `fInX`, `fRtsControl`, and `fDtrControl`. Default: `2048`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[0]);
			this->setXonLim(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (equalsIgnoreCase(function, "setXoffLim")) {
		//@InstanceCommand serial.setXoffLim
		//@Args XoffLim: int
		//@Return 
		//@Description Sets XoffLim. See `fInX`, `fRtsControl`, and `fDtrControl`. Default: `512`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[0]);
			this->setXoffLim(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (equalsIgnoreCase(function, "setXonChar")) {
		//@InstanceCommand serial.setXonChar
		//@Args XonChar: int
		//@Return 
		//@Description Sets XonChar. XonChar is an integer ASCII code, e.g. `65` for "A". Default: `17` (device control 1).
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[0]);
			this->setXonChar(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (equalsIgnoreCase(function, "setXoffChar")) {
		//@InstanceCommand serial.setXoffChar
		//@Args XoffChar: int
		//@Return 
		//@Description Sets XoffChar. XoffChar is an integer ASCII code, e.g. `65` for "A". Default: `19` (device control 3).
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[0]);
			this->setXoffChar(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (equalsIgnoreCase(function, "setErrorChar")) {
		//@InstanceCommand serial.setErrorChar
		//@Args ErrorChar: int
		//@Return 
		//@Description Sets ErrorChar. ErrorChar is an integer ASCII code, e.g. `65` for "A". Default: `0`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[0]);
			this->setErrorChar(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (equalsIgnoreCase(function, "setEofChar")) {
		//@InstanceCommand serial.setEofChar
		//@Args EofChar: int
		//@Return 
		//@Description Sets EofChar. EofChar is an integer ASCII code, e.g. `65` for "A". The character used to signal the end of data. Default: `0`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[0]);
			this->setEofChar(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (equalsIgnoreCase(function, "setEvtChar")) {
		//@InstanceCommand serial.setEvtChar
		//@Args EvtChar: int
		//@Return 
		//@Description Sets EvtChar. EvtChar is an integer ASCII code, e.g. `65` for "A". The character used to signal an event. Default: `0`.
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[0]);
			this->setEvtChar(val, ans);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (equalsIgnoreCase(function, "enableThreadedWrites")) {
		//@InstanceCommand serial.enableThreadedWrites
		//@Args 
		//@Return Success or failure message
		//@Description Attempts to begin using a separate thread for writing. May dramatically reduce the time `write` calls take to return to SQF. See the README for more information.
		this->enableWriteThread(ans);
	}
	else if (equalsIgnoreCase(function, "callbackOnChar")) {
		//@InstanceCommand serial.callbackOnChar
		//@Args charToLookFor: char
		//@Return 
		//@Description Makes the extension send data read from this port back to Arma when `charToLookFor`, specified as a `char`, is read.
		//@Description When the character is read, all data up to and **excluding** that character is sent back to Arma via the callback.
		if (argc == 0 || argv[0].length() == 0) {
			ans << "Additional argument required";
			goto end;
		}
		this->rwHandler->callbackOnChar(argv[0][0]);
	}
	else if (equalsIgnoreCase(function, "callbackOnCharCode")) {
		//@InstanceCommand serial.callbackOnCharCode
		//@Args charCodeToLookFor: int
		//@Return 
		//@Description Makes the extension send data read from this port back to Arma when the character described by `charCodeToLookFor`, specified as an ASCII char code e.g. `65` for "A", is read.
		//@Description When the character is read, all data read since the last callback, up to and **excluding** that character, is sent back to Arma via the callback.
		if (argc == 0 || argv[0].length() == 0) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[0]);
			this->rwHandler->callbackOnChar((char)val);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (equalsIgnoreCase(function, "callbackOnLength")) {
		//@InstanceCommand serial.callbackOnLength
		//@Args lengthToStopAt: int
		//@Return 
		//@Description Makes the extension send data read from this port back to Arma when the total amount of data read reaches `lengthToStopAt` characters long.
		//@Description When the target amount of data is read, all data read since the last callback is sent back to Arma via the callback.
		if (argc == 0 || argv[0].length() == 0) {
			ans << "Additional argument required";
			goto end;
		}
		try {
			int val = std::stoi(argv[0]);
			if (val < 1) {
				ans << "Length must be at least 1";
				goto end;
			}
			this->rwHandler->callbackOnLength(val);
		}
		catch (std::exception e) {
			ans << "Exception parsing input: " << e.what();
		}
	}
	else if (equalsIgnoreCase(function, "isConnected")) {
		//@InstanceCommand serial.isConnected
		//@Args 
		//@Return `true` or `false` based on whether this instance is currently connected to the port it describes
		//@Description
		ans << this->isConnected();
	}
	else if (equalsIgnoreCase(function, "connect")) {
		//@InstanceCommand serial.connect
		//@Args 
		//@Return A success or failure message
		//@Description Attempts to connect to the port described by this instance.
		this->connect(ans);
	}
	else if (equalsIgnoreCase(function, "disconnect")) {
		//@InstanceCommand serial.disconnect
		//@Args 
		//@Return A success or failure message
		//@Description Attempts to disconnect from the port described by this instance.
		//@Description If threaded writes are enabled, the extension will attempt to flush the remaining data **synchronously** before disconnecting.
		this->disconnect(ans);
	}
	else if (equalsIgnoreCase(function, "write")) {
		if (argc == 0) {
			ans << "Additional argument required";
			goto end;
		}
		//@InstanceCommand serial.write
		//@Args data: string
		//@Return A success or failure message
		//@Description Attempts to either write the data to the serial port if threaded writes are disabled, or queue the data to be written if threaded writes are enabled.
		//@Description If threaded writes are enabled, this command's return value does not say whether the data has successfully been *written*, only that it has been *queued*.
		this->write(argv[0], ans);
	}
	else {
		ans << "Unrecognized serial port instance command \"" << function << "\"";
	}
end:
	return;
}

void SerialPort::runStaticCommand(std::string function, std::string* argv, int argc, std::stringstream& ans)
{
	//used for QueryDosDevice
	char pathBuffer[800];
	if (argc == 0) {
		ans << "You must specify additional arguments";
		return;
	}
	std::string& function2 = *argv;
	if (equalsIgnoreCase(function2, "create")) {
		//@StaticCommand serial.create
		//@Args portName: string
		//@Return A success or failure message
		//@Description Creates an instance of this communication method and returns a UUID representing it.
		//@Description This command does not attempt to connect to the given port; the `connect` command must be called separately.
		//@Description Note that `portName` should be the name of the port (e.g. `COM1`), not its file (e.g. `\\.\\\\COM1`).
		SerialPort* port;
		if (argc == 1) { ans << "You must specify a port for this command"; return; }
		if (QueryDosDeviceA(argv[1].c_str(), pathBuffer, 800) == 0) {
			ans << "The specified port (\"" << argv[1] << "\") is invalid";
			return;
		}
		if (serialPorts.count(argv[1]) == 0) {
			port = new SerialPort(argv[1]);
			serialPorts[argv[1]] = port;
			commMethods[port->getID()] = port;
		}
		else {
			port = serialPorts[argv[1]];
		}
		ans << port->getID();
	}
	else if (equalsIgnoreCase(function2, "listPorts")) {
		//@StaticCommand serial.listPorts
		//@Args 
		//@Return A list of currently available COM (serial) ports in the format [[portName: string, portDriver: string], ...]
		//@Description Queries the currently available DOS COM ports using the Windows function `QueryDosDevice` from COM0 to COM254 inclusive.
		//@Description This method is marginally more computationally expensive than checking the registry, but the registry may be out of date, so this command is guaranteed to return the most up-to-date data.
		//@Description Remember to use `parseSimpleArray` since extensions can only communicate using strings.
		bool found = false;
		ans << "[";
		//loop over possible com ports, because the alternative is reading registry keys
		for (int i = 0; i < 255; i++) {
			std::string portName = "COM" + std::to_string(i);
			DWORD comTest = QueryDosDeviceA(portName.c_str(), pathBuffer, 800);
			if (comTest != 0) {
				if (found) ans << ", ";
				else found = true;
				//note: there shouldn't be, but if there are quotes in pathBuffer or portName, this will break on the SQF side
				ans << "[\"" << portName << "\", \"" << pathBuffer << "\"]";
			}
		}
		ans << "]";
	}
	else if (equalsIgnoreCase(function2, "listBaudRates")) {
		//@StaticCommand serial.listBaudRates
		//@Args 
		//@Return A list of currently available baud rates in the format [[index:int , baudRate: int], ...]
		//@Description This command simply enumerates the baud rates known to the extension; listed baud rates may not be compatible with the specific serial device.
		//@Description Remember to use `parseSimpleArray` since extensions can only communicate using strings.
		ans << "[";
		for (int i = 0; i < numRates; i++) {
			if (i != 0) ans << ", ";
			ans << "[" << i << ", " << baudNames[i] << "]";
		}
		ans << "]";
	}
	else if (equalsIgnoreCase(function2, "listStopBits")) {
		//@StaticCommand serial.listStopBits
		//@Args 
		//@Return A list of currently available stop bits in the format [[index: int, stopBits: string], ...]
		//@Description This command simply enumerates the stop bits known to the extension; listed stop bits may not be compatible with the specific serial device.
		//@Description Remember to use `parseSimpleArray` since extensions can only communicate using strings.
		ans << "[";
		for (int i = 0; i < numStopBits; i++) {
			if (i != 0) ans << ", ";
			ans << "[" << i << ", \"" << stopBitNames[i] << "\"]";
		}
		ans << "]";
	}
	else if (equalsIgnoreCase(function2, "listParities")) {
		//@StaticCommand serial.listParities
		//@Args 
		//@Return A list of currently available parities in the format [[index: int, parity: string], ...]
		//@Description This command simply enumerates the parities known to the extension; listed parities may not be compatible with the specific serial device.
		//@Description Remember to use `parseSimpleArray` since extensions can only communicate using strings.
		ans << "[";
		for (int i = 0; i < numParities; i++) {
			if (i != 0) ans << ", ";
			ans << "[" << i << ", \"" << parityNames[i] << "\"]";
		}
		ans << "]";
	}
	else if (equalsIgnoreCase(function2, "listDataBits")) {
		//@StaticCommand serial.listDataBits
		//@Args 
		//@Return A list of currently available data bits in the format [[index: int, dataBits: string], ...]
		//@Description This command simply enumerates the data bits known to the extension; listed data bits may not be compatible with the specific serial device.
		//@Description Remember to use `parseSimpleArray` since extensions can only communicate using strings.
		ans << "[";
		for (int i = 0; i < numDataBits; i++) {
			if (i != 0) ans << ", ";
			ans << "[" << i << ", \"" << dataBitNames[i] << "\"]";
		}
		ans << "]";
	}
	else if (equalsIgnoreCase(function2, "listInstances")) {
		//@StaticCommand serial.listInstances
		//@Args 
		//@Return A list of instances of this communication method in the format [[UUID: string, portName: string], ...]
		//@Description Lists extant instances of the serial communication method and their UUIDs so users have a hope of recovering their instance if they lose the UUID.
		//@Description Remember to use `parseSimpleArray` since extensions can only communicate using strings.
		ans << "[";
		auto it = commMethods.begin();
		auto end = commMethods.end();
		bool any = false;
		for (; it != end; it++) {
			auto id = (*it).first;
			auto val = (*it).second;
			if (SerialPort* port = dynamic_cast<SerialPort*>(val)) {
				if (any) ans << ", ";
				ans << "[\"" << id << "\", \"" << port->getPortNamePretty() << "\"]";
			}
		}
		ans << "]";
	}
	else {
		ans << "Unrecognized function";
	}
}

std::string SerialPort::getID()
{
	return this->id;
}
