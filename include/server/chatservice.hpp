#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/net/TcpConnection.h>
#include <map>
#include <mutex>
#include <functional>
using namespace std;
using namespace muduo;
using namespace muduo::net;

#include "redis.hpp"
#include "json.hpp"
#include "userModel.hpp"
#include "groupmodel.hpp"
#include "friendmodel.hpp"
#include "offlinemessagemodel.hpp"
using json = nlohmann::json;

//Msg是函数对象的一个类型别名，接受三个参数
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp)>;

class ChatService{
public:
    //获取单例对象的接口函数
    static ChatService* instance();

    //获得消息对应的处理器
    MsgHandler getHandler(int msgid);

    //客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);

    //处理登陆业务
    void login(const TcpConnectionPtr& conn,json &js,Timestamp time);
    
    //处理注册业务
    void reg(const TcpConnectionPtr& conn,json &js,Timestamp time);

    //一对一聊天业务
    void oneChat(const TcpConnectionPtr& conn,json &js,Timestamp time);

    //添加好友业务
    void addFriend(const TcpConnectionPtr& conn,json &js,Timestamp time);
    
    //创建群组业务
    void createGroup(const TcpConnectionPtr& conn,json &js,Timestamp time);

    //加入群组业务
    void addGroup(const TcpConnectionPtr& conn,json &js,Timestamp time);
    
    //群组聊天业务
    void groupChat(const TcpConnectionPtr& conn,json &js,Timestamp time);

    //服务器异常业务重置方法
    void reset();
    
    // 从redis消息队列中获取订阅的消息
    void handleRedisSubscribeMessage(int, string);

private:
    ChatService();

    // 存储消息id和其对应的业务处理方法
    unordered_map<int,MsgHandler> _msgHandlerMap;
    
    // 存储在线用户的通信连接
    map<int,TcpConnectionPtr> _userConnMap;

    // 定义互斥锁，保证_userConnMap的线程安全
    mutex _connMutex;

    // 数据操作类对象
    UserModel _userModel;
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;

    // 定义Redis对象
    Redis _redis;
};

#endif