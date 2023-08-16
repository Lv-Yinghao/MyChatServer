#include "redis.hpp"
#include <iostream>
using namespace std;

Redis::Redis() : _publish_context(nullptr),_subcribe_context(nullptr){}

Redis::~Redis(){
    if(_publish_context != nullptr){
        redisFree(_publish_context);
    }

    if(_subcribe_context != nullptr){
        redisFree(_subcribe_context);
    }
}

//连接Redis服务器
bool Redis::connect(){
    // 负责publish发布消息的上下文连接
    _publish_context = redisConnect("127.0.0.1",6379);
    if(_publish_context == nullptr){
        cerr << "connect redis failed!" << endl;
        return false;
    }

    // 负责subcribe订阅消息的上下文连接
    _subcribe_context = redisConnect("127.0.0.1",6379);
    if(_subcribe_context == nullptr){
        cerr << "connect redis failed!" << endl;
        return false;
    }

    // 在单独的线程中，监听通道上的时间，有消息时给业务层上报
    thread t([&](){
        observer_channel_message();
    });

    t.detach();

    cout << "connnect redis-server success" << endl;

    return true;
}

//向redis指定的通道发布消息
bool Redis::publish(int channel,string message){
    redisReply *reply = (redisReply *)redisCommand(_publish_context,"PUBLISH %d %s",channel,message.c_str());
    if(reply == nullptr){
        cerr << "publish command failed!" << endl;
        return false;
    }

    freeReplyObject(reply);
    return true;
}

//向redis指定的通道订阅消息
bool Redis::subsribe(int channel){
    // subcRIBE命令本身会造成线程阻塞等待通道里面发生消息，这里只做订阅通道，不接收通道消息
    // 通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    // 只负责发送命令，不阻塞接收redis server响应消息，否则和notifyMsg线程抢占响应资源
    if (REDIS_ERR == redisAppendCommand(this->_subcribe_context, "subcRIBE %d", channel))
    {
        cerr << "subcribe command failed!" << endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subcribe_context, &done))
        {
            cerr << "subcribe command failed!" << endl;
            return false;
        }
    }
    // redisGetReply

    return true;
}

//想redis指定的通道取消订阅消息
bool Redis::unsubsribe(int channel){
    if(REDIS_ERR == redisAppendCommand(this->_subcribe_context,"UNsubcRIBE %d",channel)){
        cerr << "unsubcribe command failed!" << endl;
        return false;
    }

    int done = 0;
    while(!done){
        if(REDIS_ERR == redisBufferWrite(this->_subcribe_context,&done)){
            cerr << "unsubcibe command failed!" << endl;
            return false;
        }
    }

    return true;
}

//在独立线程中接受订阅通道的消息
void Redis::observer_channel_message(){
    redisReply *reply = nullptr;
    while(REDIS_OK == redisGetReply(this->_subcribe_context,(void **)&reply)){
        if(reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr){
            _notify_message_handler(atoi(reply->element[1]->str),reply->element[2]->str);
        }

        freeReplyObject(reply);
    }
}

//初始化向业务层上报通道消息的回调对象
void Redis::init_notify_handler(function<void(int,string)> fn){
    this->_notify_message_handler = fn;
}