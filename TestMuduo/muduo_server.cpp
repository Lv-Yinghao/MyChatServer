#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <iostream>
#include <functional>

using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;

// 组合TcpServer对象
// 创建EventLoop=事件循环指针
// 明确TcpServer构造函数的参数
// 在构造函数中，注册链接的回调函数和读写事件的回调函数
// 设置合适的线程数量
class ChatServer
{
public:
    ChatServer(EventLoop *loop, //事件循环 
               const InetAddress &listenAddr, //IP + port
               const string &nameArg) //服务器名 
        : _server(loop, listenAddr, nameArg), _loop(loop)
    {
        //给服务器注册连接用户连接的创建和断开的回调函数
        _server.setConnectionCallback(std::bind(&ChatServer::onConnection,this,_1));
        //给服务器注册用户读写事件回调
        _server.setMessageCallback(std::bind(&ChatServer::onMessage,this,_1,_2,_3));
    
        _server.setThreadNum(4);
    }

    void start(){
        _server.start();
    }

private:
    //专门处理用户连接创建和断开
    void onConnection(const TcpConnectionPtr& conn){
        if(conn->connected()){
            cout << "connect: " << conn->peerAddress().toIpPort() << " -> " << 
            conn->localAddress().toIpPort() << endl;
        }
        else{
            cout << "disconnect: " << conn->peerAddress().toIpPort() << " -> " << 
            conn->localAddress().toIpPort() << endl;
            conn->shutdown();
        }
    }

    // 专门处理用户的读写事件
    void onMessage(const TcpConnectionPtr &conn,Buffer *buffer,Timestamp time){
        string buf = buffer->retrieveAllAsString();
        cout << "recv data:" << buf << " time: " << time.toString() << endl;
        conn->send(buf); 
    }

    TcpServer _server;
    EventLoop *_loop;
};

int main(){
    EventLoop loop;
    InetAddress addr = {"127.0.0.1",6000};

    ChatServer server(&loop,addr,"ChatServer");

    server.start();
    loop.loop();

    return 0;
}