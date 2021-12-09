#include "util.h"
#include "tcpClient.h"
#include <boost/asio.hpp>
#include <map>

//@CommMethod TCPClient
//@Description This communication method is an interface for a TCP client. It is capable of asynchronous operations and should be able to connect given any valid IP endpoint locator, including domains (e.g. "google.com", "github.com/googleben") and IP addresses (e.g. "127.0.0.1", "1.1.1.1").
//@Description Synchronous versions of operations are included, but I strongly recommend using the asynchronous versions unless you really need the output without getting it from a callback.

extern std::map<std::string, ICommunicationMethod*> commMethods;
extern ArmaCallback callback;
extern boost::asio::io_context ioContext;
std::map<std::string, TcpClient*> tcpClients;


TcpClient::TcpClient(std::string endpoint, std::string port) {
	needIOContext();
	this->id = generateUUID();
	this->endpoint = endpoint;
	this->port = port;
	this->socket = new boost::asio::ip::tcp::socket(ioContext);
	auto writeFunc = [](auto handle, auto str, auto len, auto written) {return false;};
	auto readFunc = [](boost::asio::ip::tcp::socket* handle, char* toWrite) {
		auto buff = boost::asio::buffer(toWrite, 1);
		boost::system::error_code ec;
		
		return !ec && handle->read_some(buff, ec) == 1;
	};
	this->readHandler = new ReadWriteHandler<boost::asio::ip::tcp::socket>(this->socket, this, writeFunc, readFunc, true, false);
}

TcpClient::~TcpClient()
{
	delete this->readHandler;
	delete this->socket;
}

bool TcpClient::destroy() {
	if (this->socket->is_open()) return false;
	tcpClients.erase(this->id);
	doneWithIOContext();
	return true;
}

void TcpClient::runStaticCommand(std::string function, std::string* argv, int argc, std::stringstream& ans)
{
	if (argc == 0) {
		sendFailureArr(ans, "Additional argument required");
		return;
	}
	function = *argv;
	argv++;
	argc--;
	if (equalsIgnoreCase(function, "create")) {
		//@StaticCommand TCPClient.create
		//@Args endpoint: string, port: string
		//@Return A success or failure message
		//@Description Creates an instance of this communication method and returns a UUID representing it.
		//@Description This command does not attempt to connect to the given endpoint; the `connect` command must be called separately.
		//@Description `endpoint` should be a valid, resolvable IP endpoint, consisting of first either a domain or an IP.
		//@Description Examples of valid endpoints: `127.0.0.1`, `example.com`
		if (argc < 2) { sendFailureArr(ans, "You must specify an endpoint and port for this command"); return; }
		TcpClient* client = new TcpClient(argv[0], argv[1]);
		tcpClients[argv[1]] = client;
		commMethods[client->getID()] = client;
		ans << client->getID();
	}
	else if (equalsIgnoreCase(function, "listInstances")) {
		//@StaticCommand TCPClient.listInstances
		//@Args 
		//@Return A list of instances of this communication method in the format [[UUID: string, endpoint: string], ...]
		//@Description Lists extant instances of the tcpClient communication method and their UUIDs so users have a hope of recovering their instance if they lose the UUID.
		//@Description Remember to use `parseSimpleArray` since extensions can only communicate using strings.
		ans << "[";
		auto it = commMethods.begin();
		auto end = commMethods.end();
		bool any = false;
		for (; it != end; it++) {
			auto id = (*it).first;
			auto val = (*it).second;
			if (TcpClient* client = dynamic_cast<TcpClient*>(val)) {
				if (any) ans << ", ";
				ans << "[\"" << id << "\", \"" << client->endpoint << "\"]";
			}
		}
		ans << "]";
	}
}

void TcpClient::runInstanceCommand(std::string function, std::string* argv, int argc, std::stringstream& ans)
{
	function = argv[0];
	argv++;
	argc--;
	if (equalsIgnoreCase(function, "connect")) {
		//@InstanceCommand TCPClient.connect
		//@Args 
		//@Return A success or failure message
		//@Description Attempts to connect to the endpoint described by this instance.
		//@Description Warning: This command will not return until either a connection is made or the socket times out. This will probably take a long time (at least 2 seconds) to happen, and the SQF VM will be stalled until that happens.
		boost::system::error_code ec;
		boost::asio::ip::tcp::resolver resolver(ioContext);
		boost::asio::ip::tcp::resolver::query q(this->endpoint, this->port);
		auto endpoints = resolver.resolve(q, ec);
		
		
		boost::asio::connect(*this->socket, endpoints, ec);
		
		if (ec || !this->socket->is_open()) {
			sendFailureArr(ans, "Failed to connect to endpoint: " + ec.message());
		}
		else {
			this->socket->set_option(boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO>{200});
			this->readHandler->startThreads();
			sendSuccessArr(ans, "Successfully connected to endpoint");
		}
	} else if (equalsIgnoreCase(function, "connectAsync")) {
		//@InstanceCommand TCPClient.connectAsync
		//@Args 
		//@Return 
		//@Description Attempts to connect to the endpoint described by this instance asynchronously. 
		//@Description On success or failure, the extension will call the callback with the message being the instance UUID and the data being an array with a success or failure message.
		boost::system::error_code ec;
		boost::asio::ip::tcp::resolver resolver(ioContext);
		boost::asio::ip::tcp::resolver::query q(this->endpoint, this->port);
		auto endpoints = resolver.resolve(q, ec);
		if (ec) {
			sendFailureArr(ans, "Failed to connect to endpoint: " + ec.message());
			return;
		}
		boost::asio::async_connect(*this->socket, endpoints, [this](boost::system::error_code ec, auto endp) {
			if (ec || !this->socket->is_open()) {
				callbackFailureArr(this->id, "Failed to connect to endpoint: " + ec.message());
			}
			else {
				this->socket->set_option(boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO>{200});
				this->readHandler->startThreads();
				callbackSuccessArr(this->id, "Successfully connected to endpoint");
			}
		});
	}
	else if (equalsIgnoreCase(function, "write")) {
		//@InstanceCommand TCPClient.write
		//@Args message: string
		//@Return Success or failure message
		//@Description Attempts to send `message`
		if (argc == 0) {
			sendFailureArr(ans, "Additional argument required");
			return;
		}
		if (!this->socket->is_open()) {
			sendFailureArr(ans, "Socket not connected");
			return;
		}
		boost::system::error_code ec;
		auto buff = boost::asio::buffer(argv[0]);
		auto written = this->socket->send(buff, 0, ec);
		if (ec) {
			sendFailureArr(ans, "Failed to send message: " + ec.message());
		}
		else {
			sendSuccessArr(ans, "Successfully wrote message, bytes written: " + std::to_string(written));
		}
	}
	else if (equalsIgnoreCase(function, "disconnect")) {
		//@InstanceCommand TCPClient.disconnect
		//@Args 
		//@Return A success or failure message
		//@Description Attempts to disconnect from the TCP server described by this instance.
		//@Description Any queued asynchronous operations will be canceled.
		this->readHandler->stopThreads();
		boost::system::error_code ec;
		this->socket->close(ec);
		if (ec) {
			sendFailureArr(ans, "Failed to disconnect: " + ec.message());
		}
		else {
			sendSuccessArr(ans, "Disconnected successfully.");
		}
	}
	else if (equalsIgnoreCase(function, "callbackOnChar")) {
		//@InstanceCommand TCPClient.callbackOnChar
		//@Args charToLookFor: char
		//@Return 
		//@Description Makes the extension send data read from this port back to Arma when `charToLookFor`, specified as a `char`, is read.
		//@Description When the character is read, all data up to and **excluding** that character is sent back to Arma via the callback.
		if (argc == 0 || argv[0].length() == 0) {
			sendFailureArr(ans, "Additional argument required");
			return;
		}
		this->readHandler->callbackOnChar(argv[0][0]);
	}
	else if (equalsIgnoreCase(function, "callbackOnCharCode")) {
		//@InstanceCommand TCPClient.callbackOnCharCode
		//@Args charCodeToLookFor: int
		//@Return 
		//@Description Makes the extension send data read from this port back to Arma when the character described by `charCodeToLookFor`, specified as an ASCII char code e.g. `65` for "A", is read.
		//@Description When the character is read, all data read since the last callback, up to and **excluding** that character, is sent back to Arma via the callback.
		if (argc == 0 || argv[0].length() == 0) {
			sendFailureArr(ans, "Additional argument required");
			return;
		}
		try {
			int val = std::stoi(argv[0]);
			this->readHandler->callbackOnChar((char)val);
		}
		catch (std::exception e) {
			sendFailureArr(ans, "Exception parsing input: " + std::string(e.what()));
		}
	}
	else if (equalsIgnoreCase(function, "callbackOnLength")) {
		//@InstanceCommand TCPClient.callbackOnLength
		//@Args lengthToStopAt: int
		//@Return 
		//@Description Makes the extension send data read from this port back to Arma when the total amount of data read reaches `lengthToStopAt` characters long.
		//@Description When the target amount of data is read, all data read since the last callback is sent back to Arma via the callback.
		if (argc == 0 || argv[0].length() == 0) {
			sendFailureArr(ans, "Additional argument required");
			return;
		}
		try {
			int val = std::stoi(argv[0]);
			if (val < 1) {
				sendFailureArr(ans, "Length must be at least 1");
				return;
			}
			this->readHandler->callbackOnLength(val);
		}
		catch (std::exception e) {
			sendFailureArr(ans, "Exception parsing input: " + std::string(e.what()));
		}
	}
	else if (equalsIgnoreCase(function, "isConnected")) {
		//@InstanceCommand TCPClient.isConnected
		//@Args 
		//@Return `true` or `false` based on whether this instance is currently connected to the port it describes
		//@Description
		ans << this->isConnected();
	}
}

std::string TcpClient::getID()
{
	return this->id;
}

bool TcpClient::isConnected()
{
	return this->socket->is_open();
}
