#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"
#include <string>
#include <vector>
using namespace std;

//维护群组信息的操作接口方法
class GroupModel{
public:
    // 创建群组
    bool createGroup(Group &group);

    // 加入群组
    void addGroup(int userid,int groupid,string role);

    // 查询用户所在群组消息
    vector<Group> queryGroup(int userid);
    
    // 根据指定的groupid查询群组用户ID列表
    vector<int> queryGroupUsers(int userid,int groupid);
};

#endif