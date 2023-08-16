#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>
using namespace std;
using namespace muduo;

using namespace placeholders;

//获取单例对象的接口函数
ChatService* ChatService::instance(){
    static ChatService service;
    return &service;
}

//注册消息以及对应的handler回调操作
ChatService::ChatService(){
    //  _msgHandlerMap.insert(std::bind(&ChatService::addFriend,this,_1,_2,_3));
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1, _2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1, _2,_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG,std::bind(&ChatService::oneChat,this,_1, _2,_3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG,std::bind(&ChatService::oneChat,this,_1, _2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1, _2,_3)});
}

//重置所有用户状态信息
void ChatService::reset(){
    _userModel.resetState();
}

//处理登陆业务
void ChatService::login(const TcpConnectionPtr& conn,json &js,Timestamp time){
    LOG_INFO << "do login service!!";

    int id = js["id"].get<int>();
    string password = js["password"];

    User user = _userModel.query(id);

    //user_id确实存在并且密码正确
    if(user.getId() == id && user.getPassword() == password){
        //不可重复登录
        if(user.getState() == "online"){
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "同一用户不可重复登录";
            conn->send(response.dump());
        }
        else{
            //登陆成功，记录用户连接信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id,conn}); 
            }
            
            
            //登陆成功 更新用户状态信息
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            //查询用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty()){
                response["offlinemsg"] = vec;
                //读取用户的离线消息后，将用户所在离线消息删除掉
                _offlineMsgModel.remove(id);
            }

            conn->send(response.dump());
        }
    }
    else{
        //用户不存在或者用户存在但是密码错误
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "用户名或密码错误";
        conn->send(response.dump());
    }
}
    
//处理注册业务
void ChatService::reg(const TcpConnectionPtr& conn,json &js,Timestamp time){
    LOG_INFO << "do reg service!!";
    
    string name = js["name"];
    string password = js["password"];

    User user;

    user.setName(name);
    user.setPassword(password);

    if(_userModel.insert(user)){
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else{
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

//获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid){
    auto it = _msgHandlerMap.find(msgid);
    if(it != _msgHandlerMap.end()){
        return _msgHandlerMap[msgid];
    }
    else{
        //返回lambda表达式的函数对象
        return [=](const TcpConnectionPtr& conn,json &js,Timestamp){
            LOG_ERROR << "msgid:" << msgid << " can not find hander!";
        };
    }
}

//客户端异常退出业务
void ChatService::clientCloseException(const TcpConnectionPtr &conn){
    User user;
    
    {
        lock_guard<mutex> lock(_connMutex);

        for(auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it){
            if(it->second == conn){
                // 从map表删除用户连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    

    //更新用户的状态信息
    if(user.getId() != -1){
        user.setState("offline");
        _userModel.updateState(user);
    }
}

//一对一聊天服务
void ChatService::oneChat(const TcpConnectionPtr& conn,json &js,Timestamp time){
    int toid = js["to"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end()){
            //toid在线，服务器主动推送消息给toid用户
            it->second->send(js.dump());
        }
    }

    //toid用户不在线，存储离线消息表
    _offlineMsgModel.insert(toid,js.dump());
}

//添加好友业务
void ChatService::addFriend(const TcpConnectionPtr& conn,json &js,Timestamp time){
    int userid = js["id"].get<int>();
    int friendid = js["id"].get<int>();

    _friendModel.insert(userid,friendid);
}

//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr& conn,json &js,Timestamp time){
    int useid = js["id"].get<int>();
    string desc = js["groupdesc"];
    string name = js["groupname"];

    Group group(-1,name,desc);

    _groupModel.createGroup(group);
    _groupModel.addGroup(useid,group.getId(),"creator");
}

//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr& conn,json &js,Timestamp time){
    int useid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    _groupModel.addGroup(useid,groupid,"normal");
}

//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr& conn,json &js,Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    vector<int> vec = _groupModel.queryGroupUsers(userid,groupid);

    lock_guard<mutex> lock(_connMutex);
    for(auto id : vec){
        auto it = _userConnMap.find(id);
        if(it != _userConnMap.end()){
            //toid在线，服务器主动推送消息给toid用户
            it->second->send(js.dump());
        }
        else _offlineMsgModel.insert(id,js.dump());
        // _offlineMsgModel.insert(id,js.dump());
    }
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    // 存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}