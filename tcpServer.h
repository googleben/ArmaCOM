#pragma once
#include <boost/asio.hpp>
#include <vector>
#include "util.h"
#include "ReadWriteHandler.h"
#include <mutex>

class TcpServerConnection;

class TcpServer : public ICommunicationMethod {
private:
    std::vector<TcpServerConnection*> connections;
    boost::asio::ip::tcp::acceptor* acceptor;
    std::mutex connectionsMutex;
    bool shouldListen;
    void listen();
public:
    std::string id;

    int port;

    TcpServer(int port);
    ~TcpServer();
    static void runStaticCommand(std::string function, std::string* argv, int argc, std::stringstream& ans);
    void runInstanceCommand(std::string function, std::string* argv, int argc, std::stringstream& ans);
    std::string getID();
    bool isConnected();
    bool destroy();
    void removeConnection(TcpServerConnection* conn);
};

class TcpServerConnection : public ICommunicationMethod {
private:
    bool connected;
    TcpServer* server;
    ReadWriteHandler<boost::asio::ip::tcp::socket>* readHandler;
public:
    std::string id;

    boost::asio::ip::tcp::socket socket;

    TcpServerConnection(boost::asio::ip::tcp::socket socket, TcpServer* server);
    ~TcpServerConnection();
    static void runStaticCommand(std::string function, std::string* argv, int argc, std::stringstream& ans);
    void runInstanceCommand(std::string function, std::string* argv, int argc, std::stringstream& ans);
    std::string getID();
    bool isConnected();
    boost::system::error_code disconnect();
    bool destroy();
};