#include "chatserver.hpp"
#include "chatservice.hpp"
#include "json.hpp"
#include <functional> //参数绑定器
#include <string>

using json = nlohmann::json;
using namespace placeholders;

ChatServer::ChatServer(EventLoop *loop, //事件循环 
               const InetAddress &listenAddr, //IP + port
               const string &nameArg)
        : _server(loop,listenAddr,nameArg),_loop(loop){
        //给服务器注册连接用户连接的创建和断开的回调函数
        _server.setConnectionCallback(std::bind(&ChatServer::onConnection,this,_1));
        //给服务器注册用户读写事件回调
        _server.setMessageCallback(std::bind(&ChatServer::onMessage,this,_1,_2,_3));
    
        _server.setThreadNum(4);
}

// 启动服务
void ChatServer::start(){
    _server.start();
}

void ChatServer::onConnection(const TcpConnectionPtr& conn){
    //客户端异常退出
    if(!conn->connected()){ 
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

void ChatServer::onMessage(const TcpConnectionPtr &conn,Buffer *buffer,Timestamp time){
    string buf = buffer->retrieveAllAsString();
    //数据的反序列化
    json js = json::parse(buf);
    //网络模块的代码和业务代码完全解耦合
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    // 回调消息绑定好事件处理器，执行相应的业务处理
    msgHandler(conn,js,time);
}
