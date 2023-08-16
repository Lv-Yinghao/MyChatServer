#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <unordered_map>
#include <functional>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <atomic>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

//记录当前系统登录的用户信息
User g_currentUser;

//记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;

//记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

//控制主菜单页面程序
bool isMainMenuRunning = false;

//读写线程之间的通信
sem_t rwsem;

//记录登录状态
atomic_bool g_isLoginSuccess{false};


//接收线程
void readTaskHandler(int clientfd);

//获取系统时间
string getCurrentTime();

//主聊天页面程序
void mainMenu(int);

//显示当前登陆成功的用户的基本信息
void showCurrentUserData();

int main(int argc,char ** argv){

    if(argc < 3){
        cerr << "command invalid! Example: ./CharClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    //通过命令行参数解析IP地址和端口号
    char *ip = argv[1];
    int port = atoi(argv[2]);

    //创建客户端的socketfd
    int clientfd = socket(AF_INET,SOCK_STREAM,0);
    if(clientfd == -1){
        cerr << "socket create error" << endl;
        exit(-1);
    }
    
    //填写client端需要的server信息ip和端口
    sockaddr_in server;
    memset(&server,0,sizeof (sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(port);

    //连接server
    if(connect(clientfd,(sockaddr *)&server, sizeof (sockaddr_in))){
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    //初始化读写线程用的信号量
    sem_init(&rwsem,0,0);

    //连接服务器成功 启动接受消息的子线程
    std::thread readTask(readTaskHandler,clientfd); // pthread_create
    readTask.detach(); // pthread_detach

    while(1){
        cout << "========================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "========================" << endl;
        cout << "choice:";

        int choice = 0;
        cin >> choice;
        cin.get(); //读取缓冲区的多余回车

        switch(choice){
            //登录业务
            case 1:{
                int id = 0;
                char pwd[50] = {0};
                cout << "userid:";
                cin >> id;
                cin.get();
                cout << "userpassword:";
                cin.getline(pwd,50);

                json js;
                js["msg"] = LOGIN_MSG;
                js["id"] = id;
                js["password"] = pwd;
                string request = js.dump();

                puts("0");

                g_isLoginSuccess = false;
                
                int len = send(clientfd, request.c_str(), strlen(request.c_str())+1,0);
                puts("1");
                
                if(len == -1){
                    cerr << "send login msg error:" << request << endl;
                }

                puts("2");

                //等待信号量，子线程处理完登录的响应消息后，通知这里进行处理
                sem_wait(&rwsem);

                if(g_isLoginSuccess){
                    //进入聊天主菜单页面
                    isMainMenuRunning = true;
                    mainMenu(clientfd);
                }
            }
            break;
            //register业务
            case 2:{
                char name[50] = {0};
                char pwd[50] = {0};
                cout << "username:";
                cin.getline(name,50);
                cout << "userpassword:";
                cin.getline(pwd,50);

                json js;
                js["msgid"] = REG_MSG;
                js["name"] = name;
                js["password"] = pwd;
                string request = js.dump();

                int len = send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
                if(len == -1){
                    cerr << "send login msg error:" << request << endl;
                }

                //等待信号量，子线程处理完注册的响应消息后，通知这里进行处理
                sem_wait(&rwsem);
            }
            break;
            // quit业务
            case 3:{
                close(clientfd);
                sem_destroy(&rwsem);
                exit(0);
            }
            default:
                cerr << "invalid input!" << endl;
                break;
        }
    }

    return 0;
}

//处理注册的响应逻辑
void doRegResponse(json &responsejs){
    //注册失败
    if(responsejs["errno"].get<int>() != 0){
        cerr << "name is already exist, register error!" << endl;
    }
    //注册地功
    else{
        cout << "name register success,userid is " << responsejs["id"]
            << ",do not forget it!" << endl;
    } 
}

//处理登录的相应逻辑
void doLoginResponse(json &responsejs){
    //登录失败
    if(responsejs["errno"].get<int>() != 0){
        cerr << responsejs["errmsg"] << endl;
        g_isLoginSuccess = false;
    }
    //登录成功
    else{
        //记录当前用户的ID和名称
        g_currentUser.setId(responsejs["id"].get<int>());
        g_currentUser.setName(responsejs["name"]);

        //记录当前用户的好友列表信息
        if(responsejs.contains("friends")){
            //初始化
            g_currentUserFriendList.clear();

            vector<string> vec = responsejs["friends"];
            for(string &str : vec){
                json js = json::parse(str);

                User user;
                user.setId(js["id"].get<int>());
                user.setName(js["name"]);
                user.setState(js["state"]);
                g_currentUserFriendList.push_back(user);
            }
        }

        //记录当前用户的好友列表信息
        if(responsejs.contains("groups")){
            //初始化
            g_currentUserGroupList.clear();

            vector<string> vec = responsejs["groups"];
            for(string &str : vec){
                json js = json::parse(str);

                Group group;
                group.setId(js["id"].get<int>());
                group.setName(js["groupname"]);
                group.setDesc(js["groupdesc"]);

                vector<string> vec1 = responsejs["groups"];
                for(string &userstr : vec1){
                    json groupjs = json::parse(userstr);

                    GroupUser user;
                    json js = json::parse(userstr);
                    user.setId(js["id"].get<int>());
                    user.setName(js["name"]);
                    user.setState(js["state"]);
                    user.setRole(js["role"]);
                    group.getUsers().push_back(user);
                }


                g_currentUserGroupList.push_back(group);
            }
        }

        //显示登录用户的基本信息
        showCurrentUserData();

        //显示当前用户的离线信息
        if(responsejs.contains("offlinemsg")){
            vector<string> vec = responsejs["offlinemsg"];

            for(string &str : vec){
                json js = json::parse(str);

                if(ONE_CHAT_MSG == js["msgid"].get<int>()){
                    cout << js["time"].get<string>() << " [" << js["id"] << "]"
                    << js["name"].get<string>() << " said: " << js["msg"].get<string>() << endl;
                }
                else{
                    cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << " ["
                    << js["id"] << "]" << js["name"].get<string>() << " said: " << js["msg"].get<string>() << endl;
                }
            }
        }

        g_isLoginSuccess = true;
    } 
}

//子线程 - 接收线程
void readTaskHandler(int clientfd){
    while(1){
        char buffer[1024] = {0};
        int len = recv(clientfd,buffer,1024,0);

        if(len == -1 || len == 0){
            close(clientfd);
            exit(-1);
        }

        //接受ChatServer转发的数据，反序列化生成json对象
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();

        puts("222");

        if(ONE_CHAT_MSG == msgtype){
            cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
        }
        else if(GROUP_CHAT_MSG == msgtype){
            cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
        }
        else if(LOGIN_MSG_ACK == msgtype){
            doLoginResponse(js);
            sem_post(&rwsem);
        }
        else if(REG_MSG_ACK == msgtype){
            doRegResponse(js);
            sem_post(&rwsem);
        }

        puts("333");
    }
}

//显示当前登陆成功用户的基本信息
void showCurrentUserData(){
    cout << "======================login user======================" << endl;
    cout << "current login user => id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout << "----------------------friend list---------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "----------------------group list----------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState()
                     << " " << user.getRole() << endl;
            }
        }
    }
    cout << "======================================================" << endl;
}

void help(int fd = 0,string str = "");

void chat(int,string);

void addfriend(int,string);

void creategroup(int,string);

void addgroup(int,string);

void groupchat(int,string);

void loginout(int,string);

//系统支持的客户端命令列表
unordered_map<string,string> commandMap = {
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat:friendid:message"},
    {"addfriend", "添加好友，格式addfriend:friendid"},
    {"creategroup", "创建群组，格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式addgroup:groupid"},
    {"groupchat", "群聊，格式groupchat:groupid:message"},
    {"loginout", "注销，格式loginout"}
};

//注册系统支持的客户端命令处理
unordered_map<string,function<void(int,string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}
};

//主页面聊天程序
void mainMenu(int clientfd){
    help();

    char buffer[1024] = {0};

    while(isMainMenuRunning) {
        cin.getline(buffer,1024);
        string commandbuf(buffer);
        string command;
        int idx = commandbuf.find(":");

        //截取第一段命令
        if(idx == -1){
            command = commandbuf;
        }
        else{
            command = commandbuf.substr(0,idx);
        }
        //找不到对应的命令
        auto it = commandHandlerMap.find(command);
        if(it == commandHandlerMap.end()){
            cerr << "invalid input command!" << endl;
            continue;
        }

        //调用相应的命令事件处理回调
        it->second(clientfd,commandbuf.substr(idx+1,commandbuf.size() - idx));
    }   
}   

void help(int,string){
    cout << "show command list >>> " << endl;
    for(auto & p : commandMap){
        cout << p.first << " : " << p.second << endl;
    }
    puts("");
}

void chat(int clientfd,string str){
    int idx = str.find(":");
    if(idx == -1){
        cerr << "chat command invalid!" << endl;
        return;
    }

    string message = str.substr(idx+1,str.size()-idx);
    int friendid = atoi(str.substr(0,idx).c_str());

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["msg"] = message;
    js["toid"] = friendid;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len == -1){
        cerr << "send chat msg error ->" << buffer << endl;
    }
}

void addfriend(int clientfd,string str){
    int friendid = atoi(str.c_str());

    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);

    if(len == -1){
        cerr << "send addfriend msg error ->" << buffer << endl;
    }
}

void creategroup(int clientfd,string str){
    int idx = str.find(":");
    if(idx == -1){
        cerr << "creategroup command invalid!" << endl;
        return;
    }

    string groupname = str.substr(0,idx);
    string groupdesc = str.substr(idx+1,str.size()-idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = g_currentUser.getId();
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);

    if(len == -1){
        cerr << "send creategroup msg error ->" << buffer << endl;
    }
}

void addgroup(int clientfd,string str){
    int groupid = atoi(str.c_str());

    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);

    if(len == -1){
        cerr << "send addgroup msg error ->" << buffer << endl;
    }
}

void groupchat(int clientfd,string str){
    int idx = str.find(":");
    if(idx == -1){
        cerr << "groupchat command invalid!" << endl;
        return;
    }

    int groupid = atoi(str.substr(0,idx).c_str());
    string message = str.substr(idx+1,str.size()-idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1){
        cerr << "send groupchat msg error ->" << buffer << endl;
    }
}

void loginout(int clientfd,string str){
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);

    if(len == -1){
        cerr << "send loginout msg error ->" << buffer << endl;
    }
    else{
        isMainMenuRunning = false;
    }
}

// 获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}