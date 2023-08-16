#include "groupmodel.hpp"
#include "db.h"


// 创建群组
bool GroupModel::createGroup(Group &group){
    char sql[1024] = {0};

    sprintf(sql,"insert into allgroup(groupname,groupdesc) values('%s','%s')", group.getName().c_str(),group.getDesc().c_str());

    MySQL mysql;
    if(mysql.connect()){
        if(mysql.update(sql)){
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }

    return false;
}

// 加入群组
void GroupModel::addGroup(int userid,int groupid,string role){
    char sql[1024] = {0};

    sprintf(sql,"insert into groupuser values(%d ,%d,'%s')",userid,groupid,role.c_str());

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }

}

// 查询用户所在群组消息
vector<Group> GroupModel::queryGroup(int userid){
    /*
        1.先根据userid在groupuser表中查询用户所属的群组信息
        2.根据群组信息，查询属于群组的所有用户的userid，并且和user表进行多表联合查询
    */
    char sql[1024] = {0};

    vector<Group> vec;
    sprintf(sql,"select a.id,a.groupname,a.groupdesc from allgroup a join groupuser b on a.id = b.groupid where userid = %d",userid);

    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES * res = mysql.query(sql);

        if(res != nullptr){
            MYSQL_ROW row;
            while(((row = mysql_fetch_row(res)) != nullptr)){
                Group group;
                //查询userid的所有群组信息
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                vec.push_back(group);
            }

            mysql_free_result(res);
        }
    }

    //查询群组的用户信息
    for(Group &group : vec){
        sprintf(sql,"select a.id,a.name,a.state,b.grouprole from user a join \
        groupuser b on a.id = b.userid where b.userid = %d",group.getId());
        
        MYSQL_RES * res = mysql.query(sql);

        if(res != nullptr){
            MYSQL_ROW row;
            while(((row = mysql_fetch_row(res)) != nullptr)){
                GroupUser user;
                //查询userid的所有群组信息
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                user.setRole(row[3]);
                group.getUsers().push_back(user);
            }

            mysql_free_result(res);
        }
    }

    return vec;
}

//根据指定的groupid查询群组用户ID列表
vector<int> GroupModel::queryGroupUsers(int userid,int groupid){
    char sql[1024] = {0};
    vector<int> vec;

    sprintf(sql,"select userid from groupuser where userid != %d and groupid = %d",userid,groupid);
    
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES * res = mysql.query(sql);

        if(res != nullptr){
            MYSQL_ROW row;
            while(((row = mysql_fetch_row(res)) != nullptr)){
                vec.push_back(atoi(row[0]));
            }

            mysql_free_result(res);
        }
    }

    return vec;
}
