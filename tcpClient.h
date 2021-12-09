#pragma once
#include <boost/asio.hpp>
#include "util.h"
#include "ReadWriteHandler.h"

class TcpClient :
    public ICommunicationMethod
{
private:
    ReadWriteHandler<boost::asio::ip::tcp::socket>* readHandler;
public:
    std::string id;
    
    std::string endpoint;
    std::string port;
    boost::asio::ip::tcp::socket* socket;

    TcpClient(std::string endpointIp, std::string port);
    ~TcpClient();
    static void runStaticCommand(std::string function, std::string* argv, int argc, std::stringstream& ans);
    void runInstanceCommand(std::string function, std::string* argv, int argc, std::stringstream& ans);
    std::string getID();
    bool isConnected();
    bool destroy();
};

