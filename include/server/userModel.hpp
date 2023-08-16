#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"

class UserModel{
public:
    //插入用户
    bool insert(User &user);
    //查找用户
    User query(int id);
    //更新用户状态
    bool updateState(User user);
    //重置用户状态信息
    void resetState();
};

#endif