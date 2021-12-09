#include "tcpServer.h"

//@CommMethod TCPServer
//@Description This communication method is an interface for a TCP server. It can asynchronously listen for incoming connections on a provided port and return TCPServerConnections representing active connections.

//@CommMethod TCPServerConnection
//@Description This communication method is used for TCP connections to a TCPServer communication method. This comm method cannot be directly created; it is only created when a remote client connects to a TCPServer.
extern std::map<std::string, ICommunicationMethod*> commMethods;
extern ArmaCallback callback;
extern boost::asio::io_context ioContext;

using boost::asio::ip::tcp;

std::map<int, TcpServer*> tcpServers;

TcpServer::TcpServer(int port)
{
	needIOContext();
	this->id = generateUUID();
	this->port = port;
	this->acceptor = new tcp::acceptor(ioContext, tcp::endpoint(tcp::v4(), port));
}

TcpServer::~TcpServer()
{
	delete this->acceptor;
}

void TcpServer::runStaticCommand(std::string function, std::string* argv, int argc, std::stringstream& ans)
{
	if (argc == 0) {
		sendFailureArr(ans, "Additional argument required");
		return;
	}
	function = *argv;
	argv++;
	argc--;
	if (equalsIgnoreCase(function, "create")) {
		//@StaticCommand TCPServer.create
		//@Args port: int
		//@Return A success or failure message
		//@Description Creates an instance of this communication method and returns a UUID representing it.
		//@Description This command does not attempt to listen for connections; you must initiate that separately.
		if (argc < 1) { sendFailureArr(ans, "You must specify a port for this command"); return; }
		int port;
		try {
			port = std::stoi(argv[0]);
		}
		catch (std::exception e) {
			sendFailureArr(ans, "Exception parsing input: " + std::string(e.what()));
			return;
		}
		if (tcpServers.find(port) != tcpServers.end()) {
			sendFailureArr(ans, "A TCP Server already exists for the given port");
			return;
		}
		TcpServer* server = new TcpServer(port);
		tcpServers[port] = server;
		commMethods[server->getID()] = server;
		ans << server->getID();
	}
	else if (equalsIgnoreCase(function, "listInstances")) {
		//@StaticCommand TCPServer.listInstances
		//@Args 
		//@Return A list of instances of this communication method in the format [[UUID: string, port: string], ...]
		//@Description Lists extant instances of the TCPServer communication method and their UUIDs so users have a hope of recovering their instance if they lose the UUID.
		//@Description Remember to use `parseSimpleArray` since extensions can only communicate using strings.
		ans << "[";
		auto it = commMethods.begin();
		auto end = commMethods.end();
		bool any = false;
		for (; it != end; it++) {
			auto id = (*it).first;
			auto val = (*it).second;
			if (TcpServer* server = dynamic_cast<TcpServer*>(val)) {
				if (any) ans << ", ";
				ans << "[\"" << id << "\", \"" << server->port << "\"]";
			}
		}
		ans << "]";
	}
}

void TcpServer::listen() {
	if (!this->shouldListen) return;
	this->acceptor->async_accept([this](auto ec, boost::asio::ip::tcp::socket newConn) {
		if (ec) {
			if (this->shouldListen) callbackFailureArr(this->id, "Failed to listen for connections: " + ec.message());
		}
		else {
			TcpServerConnection* client = new TcpServerConnection(std::move(newConn), this);
			this->connectionsMutex.lock();
			this->connections.push_back(client);
			this->connectionsMutex.unlock();
			auto data = ("[\""+this->id+"\", \"" + client->getID() + "\"]");
			while (callback("ArmaCOM", "new_tcp_connection", data.c_str()) == -1) Sleep(1);
			this->listen();
		}
	});
}

void TcpServer::runInstanceCommand(std::string function, std::string* argv, int argc, std::stringstream& ans)
{
	function = argv[0];
	argv++;
	argc--;
	if (equalsIgnoreCase(function, "listen")) {
		//@InstanceCommand TCPServer.listen
		//@Args 
		//@Return A success or failure message
		//@Description Attempts to start listening for new TCP connections on this server's port.
		//@Description The listening process is asynchronous, and connections will be reported via callback, with the function being "new_tcp_connection", and the args taking this form: `[serverID: string, newConnectionID: string]`
		//@Description If the attempt fails at a later stage during the async pipeline, the failure will be reported along with an error message, with the function being "FAILURE" and the args taking this form: `[serverID: string, message: string]`
		//@Description As mentioned earlier, connections are of the communication method TCPServerConnection.
		boost::system::error_code ec;
		this->acceptor->listen(10, ec);
		if (ec) {
			sendFailureArr(ans, "Failed to listen: " + ec.message());
			return;
		}
		this->shouldListen = true;
		this->listen();
		sendSuccessArr(ans, "Started listening");
	}
	else if (equalsIgnoreCase(function, "stopListening")) {
		//@InstanceCommand TCPServer.stopListening
		//@Args 
		//@Return A success or failure message
		//@Description Stops listening for new connections.
		this->shouldListen = false;
		boost::system::error_code ec;
		this->acceptor->cancel(ec);
		if (!ec) this->acceptor->close(ec);
		if (ec) {
			sendFailureArr(ans, "Failed to cancel accept: " + ec.message());
		}
		else {
			sendSuccessArr(ans, "Successfully cancelled accept operation");
		}
	}
	else if (equalsIgnoreCase(function, "disconnectAll")) {
		//@InstanceCommand TCPServer.disconnectAll
		//@Args 
		//@Return A success or failure message
		//@Description Attempts to disconnect and destroy all existing connections to this server.
		//@Description All clients must be disconnected before destroying a server, so call this function before attempting to destroy a TCPServer if you're not sure if there's still connections.

		//copy the list of connections so we don't have any issues like deadlocks
		this->connectionsMutex.lock();
		std::vector<TcpServerConnection*> tmp(this->connections);
		this->connectionsMutex.unlock();
		size_t len = tmp.size();
		//I think it's technically possible to have a use-after-free of some kind here, but
		//unless I'm sorely mistaken, it should only be possible in some kind of crazy edge case
		//that isn't worth worrying about unless people start using the extension in weird ways.
		for (auto it = tmp.begin(); it < tmp.end(); it++) {
			auto c = *it;
			auto id = c->getID();
			auto ec = c->disconnect();
			if (ec) {
				sendFailureArr(ans, "Failed to disconnect a client: " + ec.message());
			}
			commMethods.erase(id);
			c->destroy();
			delete c;
		}
		sendSuccessArr(ans, "Disconnected and destroyed all " + std::to_string(len) + " connections.");
	}
}

std::string TcpServer::getID()
{
	return this->id;
}

bool TcpServer::isConnected()
{
	return this->acceptor->is_open();
}

bool TcpServer::destroy()
{
	if (!this->connections.empty()) return false;
	doneWithIOContext();
	return true;
}

void TcpServer::removeConnection(TcpServerConnection* conn)
{
	this->connectionsMutex.lock();
	size_t i = 0;
	size_t len = this->connections.size();
	for (; i < len; i++) {
		if (this->connections[i] == conn) break;
	}
	//in case somehow the connection isn't listed
	if (i == len) {
		this->connectionsMutex.unlock();
		return;
	}
	//if it's not already the last element, copy the current last element to
	//take the place of the one we want to forget
	if (i != len - 1) {
		this->connections[i] = this->connections[len - 1];
	}
	this->connections.pop_back();
	this->connectionsMutex.unlock();
}

TcpServerConnection::TcpServerConnection(boost::asio::ip::tcp::socket sock, TcpServer* serv) : socket(std::move(sock)), server(serv)
{
	needIOContext();
	this->id = generateUUID();
	commMethods[this->id] = this;
	auto writeFunc = [](auto handle, auto str, auto len, auto written) {return false; };
	auto readFunc = [](boost::asio::ip::tcp::socket* handle, char* toWrite) {
		auto buff = boost::asio::buffer(toWrite, 1);
		boost::system::error_code ec;

		return !ec && handle->read_some(buff, ec) == 1;
	};
	this->readHandler = new ReadWriteHandler<boost::asio::ip::tcp::socket>(&this->socket, this, writeFunc, readFunc, true, false);
	this->readHandler->startThreads();
	this->connected = true;
}

TcpServerConnection::~TcpServerConnection()
{
}

void TcpServerConnection::runStaticCommand(std::string function, std::string* argv, int argc, std::stringstream& ans)
{
	//no static commands for this comm method, it can only be created through TcpServer
}

boost::system::error_code TcpServerConnection::disconnect() {
	boost::system::error_code ec;
	if (!this->connected) return ec;
	this->socket.close(ec);
	if (ec) return ec;
	//we must manually call the destructor because otherwise the IOContext will still
	//have a "valid" handle to this socket, even after this TcpServerConnection has been destroyed
	//and the memory is invalid, leading to a guaranteed use-after-free when the IOContext is destroyed.
	this->socket.~basic_stream_socket();
	this->readHandler->stopThreads();
	this->connected = false;
	return ec;
}

void TcpServerConnection::runInstanceCommand(std::string function, std::string* argv, int argc, std::stringstream& ans)
{
	function = argv[0];
	argv++;
	argc--;
	if (equalsIgnoreCase(function, "getEndpoint")) {
		//@InstanceCommand TCPServerConnection.write
		//@Args
		//@Return The remote endpoint this connection is to
		//@Description Returns the name of the remote endpoint this connection is to as described by the underlying socket.
		//@Description The returned value will be the endpoint's name and the (remote) port, separated by a colon, e.g. `127.0.0.1:8080`.
		if (!this->connected) sendFailureArr(ans, "This connection has already destroyed its socket");
		boost::system::error_code ec;
		auto ep = this->socket.remote_endpoint(ec);
		if (ec) {
			sendFailureArr(ans, "Failed to get the remote endpoint: " + ec.message());
			return;
		}
		auto addr = ep.address();
		auto port = ep.port();
		ans << addr.to_string() << ":" << std::to_string(port);
	}
	else if (equalsIgnoreCase(function, "isIPv6")) {
		//@InstanceCommand TCPServerConnection.isIPv6
		//@Args
		//@Return Whether or not this connection is IPv6
		//@Description Returns `true` if this connection is over IPv6, and `false` otherwise
		if (!this->connected) sendFailureArr(ans, "This connection has already destroyed its socket");
		boost::system::error_code ec;
		auto ep = this->socket.remote_endpoint(ec);
		if (ec) {
			sendFailureArr(ans, "Failed to get the remote endpoint: " + ec.message());
			return;
		}
		ans << ep.address().is_v6();
	}
	else if (equalsIgnoreCase(function, "write")) {
		//@InstanceCommand TCPServerConnection.write
		//@Args message: string
		//@Return Success or failure message
		//@Description Attempts to send `message`
		if (argc == 0) {
			sendFailureArr(ans, "Additional argument required");
			return;
		}
		if (!this->connected) sendFailureArr(ans, "This connection has already destroyed its socket");
		if (!this->socket.is_open()) {
			sendFailureArr(ans, "Socket not connected");
			return;
		}
		boost::system::error_code ec;
		auto buff = boost::asio::buffer(argv[0]);
		auto written = this->socket.send(buff, 0, ec);
		if (ec) {
			sendFailureArr(ans, "Failed to send message: " + ec.message());
		}
		else {
			sendSuccessArr(ans, "Successfully wrote message, bytes written: " + std::to_string(written));
		}
	}
	else if (equalsIgnoreCase(function, "disconnect")) {
		//@InstanceCommand TCPServerConnection.disconnect
		//@Args 
		//@Return A success or failure message
		//@Description Attempts to disconnect from the remote client.
		//@Description Any queued asynchronous operations will be canceled.
		
		auto ec = this->disconnect();
		
		if (ec) {
			sendFailureArr(ans, "Failed to disconnect: " + ec.message());
		}
		else {
			sendSuccessArr(ans, "Disconnected successfully.");
		}
	}
	else if (equalsIgnoreCase(function, "callbackOnChar")) {
		//@InstanceCommand TCPServerConnection.callbackOnChar
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
		//@InstanceCommand TCPServerConnection.callbackOnCharCode
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
		//@InstanceCommand TCPServerConnection.callbackOnLength
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
		//@InstanceCommand TCPServerConnection.isConnected
		//@Args 
		//@Return `true` or `false` based on whether this instance is currently connected to the port it describes
		//@Description
		ans << this->isConnected();
	}
}

std::string TcpServerConnection::getID()
{
	return this->id;
}

bool TcpServerConnection::isConnected()
{
	return this->socket.is_open();
}

bool TcpServerConnection::destroy()
{
	if (this->socket.is_open()) return false;
	this->server->removeConnection(this);
	doneWithIOContext();
	return true;
}
