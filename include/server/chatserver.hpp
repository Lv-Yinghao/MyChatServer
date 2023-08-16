#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
using namespace std;
using namespace muduo;
using namespace muduo::net;

class ChatServer{
public:
    ChatServer(EventLoop *loop, //事件循环 
               const InetAddress &listenAddr, //IP + port
               const string &nameArg);

    void start();

private:
    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr &conn,Buffer *buffer,Timestamp time);

    TcpServer _server;
    EventLoop *_loop;
};

#endif