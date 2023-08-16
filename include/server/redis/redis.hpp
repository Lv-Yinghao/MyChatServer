#ifndef REDIS_H
#define REDIS_H

#include <hiredis/hiredis.h>
#include <thread>
#include <functional>
using namespace std;

class Redis{
public:
    Redis();
    ~Redis();

    //连接Redis服务器
    bool connect();

    //向redis指定的通道发布消息
    bool publish(int channel,string message);

    //向redis指定的通道订阅消息
    bool subsribe(int channel);

    //想redis指定的通道取消订阅消息
    bool unsubsribe(int channel);

    //在独立线程中接受订阅通道的消息
    void observer_channel_message();

    //初始化向业务层上报通道消息的回调对象
    void init_notify_handler(function<void(int,string)> fn);

private:
    //hiredis同步上下文对象，负责publish消息
    redisContext *_publish_context;

    //hiredis同步上下文对象，负责subsribe消息
    redisContext *_subcribe_context;

    //回调操作，收到订阅的消息
    function<void(int,string)> _notify_message_handler;
};

#endif