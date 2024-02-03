#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <unordered_map>
#include <functional>

using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 记录当前系统登录的用户信息
User current_user;
// 记录当前登录用户的好友列表信息
std::vector<User> current_user_friend_list;
// 记录当前登录用户的群组列表信息
std::vector<Group> current_user_group_list;
// 显示当前登录成功用户的基本信息
void ShowCurrentUserData();
// 控制主菜单页面程序
bool is_main_menu_running = false;


// 接收线程
void ReadTaskHandler(int clientfd);
// 获取系统时间（聊天信息需要添加时间信息）
std::string GetCurrentTime();
// 主聊天页面程序
void MainMenu(int);

// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000" << std::endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        std::cerr << "socket create error" << std::endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server进行连接
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        std::cerr << "connect server error" << std::endl;
        close(clientfd);
        exit(-1);
    }

    // main线程用于接收用户输入，负责发送数据
    for (;;)
    {
        // 显示首页面菜单 登录、注册、退出
        std::cout << "========================" << std::endl;
        std::cout << "1. login" << std::endl;
        std::cout << "2. register" << std::endl;
        std::cout << "3. quit" << std::endl;
        std::cout << "========================" << std::endl;
        std::cout << "choice:";
        int choice = 0;
        std::cin >> choice;
        std::cin.get(); // 读掉缓冲区残留的回车

        switch (choice)
        {
        case 1: // login业务
        {
            int id = 0;
            char pwd[50] = {0};
            std::cout << "userid:";
            std::cin >> id;
            std::cin.get(); // 读掉缓冲区残留的回车
            std::cout << "userpassword:";
            std::cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            std::string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                std::cerr << "send login msg error:" << request << std::endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, 1024, 0);
                if (-1 == len)
                {
                    std::cerr << "recv login response error" << std::endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    if (0 != responsejs["errno"].get<int>()) // 登录失败
                    {
                        std::cerr << responsejs["errmsg"] << std::endl;
                    }
                    else // 登录成功
                    {
                        // 记录当前用户的id和name
                        current_user.SetId(responsejs["id"].get<int>());
                        current_user.SetName(responsejs["name"]);

                        // 记录当前用户的好友列表信息
                        if (responsejs.contains("friends"))
                        {
                            // 初始化
                            current_user_friend_list.clear();

                            std::vector<std::string> vec = responsejs["friends"];
                            for (std::string &str : vec)
                            {
                                json js = json::parse(str);
                                User user;
                                user.SetId(js["id"].get<int>());
                                user.SetName(js["name"]);
                                user.SetState(js["state"]);
                                current_user_friend_list.push_back(user);
                            }
                        }

                        // 记录当前用户的群组列表信息
                        if (responsejs.contains("groups"))
                        {
                            // 初始化
                            current_user_group_list.clear();

                            std::vector<std::string> vec1 = responsejs["groups"];
                            for (std::string &groupstr : vec1)
                            {
                                json grpjs = json::parse(groupstr);
                                Group group;
                                group.SetId(grpjs["id"].get<int>());
                                group.SetName(grpjs["groupname"]);
                                group.SetDesc(grpjs["groupdesc"]);

                                std::vector<std::string> vec2 = grpjs["users"];
                                for (std::string &userstr : vec2)
                                {
                                    GroupUser user;
                                    json js = json::parse(userstr);
                                    user.SetId(js["id"].get<int>());
                                    user.SetName(js["name"]);
                                    user.SetState(js["state"]);
                                    user.SetRole(js["role"]);
                                    group.GetUsers().push_back(user);
                                }

                                current_user_group_list.push_back(group);
                            }
                        }

                        // 显示登录用户的基本信息
                        ShowCurrentUserData();

                        // 显示当前用户的离线消息  个人聊天信息或者群组消息
                        if (responsejs.contains("offlinemsg"))
                        {
                            std::vector<std::string> vec = responsejs["offlinemsg"];
                            for (std::string &str : vec)
                            {
                                json js = json::parse(str);
                                // time + [id] + name + " said: " + xxx
                                if (ONE_CHAT_MSG == js["msgid"].get<int>())
                                {
                                    std::cout << js["time"].get<std::string>() << " [" << js["id"] << "]" << js["name"].get<std::string>()
                                         << " said: " << js["msg"].get<std::string>() << std::endl;
                                }
                                else
                                {
                                    std::cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<std::string>() << " [" << js["id"] << "]" << js["name"].get<std::string>()
                                         << " said: " << js["msg"].get<std::string>() << std::endl;
                                }
                            }
                        }

                        // 登录成功，启动接收线程负责接收数据，该线程只启动一次
                        static int read_thread_number = 0;
                        if(read_thread_number == 0)
                        {
                            std::thread ReadTask(ReadTaskHandler, clientfd); // pthread_create
                            ReadTask.detach();                               // pthread_detach
                            read_thread_number++;
                        }
                        
                        // 进入聊天主菜单页面
                        is_main_menu_running = true;
                        MainMenu(clientfd);
                    }
                }
            }
        }
        break;
        case 2: // register业务
        {
            char name[50] = {0};
            char pwd[50] = {0};
            std::cout << "username:";
            std::cin.getline(name, 50);
            std::cout << "userpassword:";
            std::cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            std::string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                std::cerr << "send reg msg error:" << request << std::endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, 1024, 0);
                if (-1 == len)
                {
                    std::cerr << "recv reg response error" << std::endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    if (0 != responsejs["errno"].get<int>()) // 注册失败
                    {
                        std::cerr << name << " is already exist, register error!" << std::endl;
                    }
                    else // 注册成功
                    {
                        std::cout << name << " register success, userid is " << responsejs["id"]
                             << ", do not forget it!" << std::endl;
                    }
                }
            }
        }
        break;
        case 3: // quit业务
            close(clientfd);
            exit(0);
        default:
            std::cerr << "invalid input!" << std::endl;
            break;
        }
    }

    return 0;
}

// 接收线程
void ReadTaskHandler(int clientfd)
{
    for (;;)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);  // 阻塞了
        if (-1 == len || 0 == len)
        {
            close(clientfd);
            exit(-1);
        }

        // 接收ChatServer转发的数据，反序列化生成json数据对象
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if (ONE_CHAT_MSG == msgtype)
        {
            std::cout << js["time"].get<std::string>() << " [" << js["id"] << "]" << js["name"].get<std::string>()
                 << " said: " << js["msg"].get<std::string>() << std::endl;
            continue;
        }

        if (GROUP_CHAT_MSG == msgtype)
        {
            std::cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<std::string>() << " [" << js["id"] << "]" << js["name"].get<std::string>()
                 << " said: " << js["msg"].get<std::string>() << std::endl;
            continue;
        }
    }
}

// 显示当前登录成功用户的基本信息
void ShowCurrentUserData()
{
    std::cout << "======================login user======================" << std::endl;
    std::cout << "current login user => id:" << current_user.GetId() << " name:" << current_user.GetName() << std::endl;
    std::cout << "----------------------friend list---------------------" << std::endl;
    if (!current_user_friend_list.empty())
    {
        for (User &user : current_user_friend_list)
        {
            std::cout << user.GetId() << " " << user.GetName() << " " << user.GetState() << std::endl;
        }
    }
    std::cout << "----------------------group list----------------------" << std::endl;
    if (!current_user_group_list.empty())
    {
        for (Group &group : current_user_group_list)
        {
            std::cout << group.GetId() << " " << group.GetName() << " " << group.GetDesc() << std::endl;
            for (GroupUser &user : group.GetUsers())
            {
                std::cout << user.GetId() << " " << user.GetName() << " " << user.GetState()
                     << " " << user.GetRole() << std::endl;
            }
        }
    }
    std::cout << "======================================================" << std::endl;
}

// "help" command handler
void help(int fd = 0, std::string str = "");
// "chat" command handler
void chat(int, std::string);
// "addfriend" command handler
void addfriend(int, std::string);
// "creategroup" command handler
void creategroup(int, std::string);
// "addgroup" command handler
void addgroup(int, std::string);
// "groupchat" command handler
void groupchat(int, std::string);
// "loginout" command handler
void loginout(int, std::string);

// 系统支持的客户端命令列表
std::unordered_map<std::string, std::string> command_map = {
    {"help", "显示所有支持的命令, 格式help"},
    {"chat", "一对一聊天, 格式chat:friendid:message"},
    {"addfriend", "添加好友, 格式addfriend:friendid"},
    {"creategroup", "创建群组, 格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组, 格式addgroup:groupid"},
    {"groupchat", "群聊, 格式groupchat:groupid:message"},
    {"loginout", "注销, 格式loginout"}
};

// 注册系统支持的客户端命令处理
std::unordered_map<std::string, std::function<void(int, std::string)>> command_handler_map = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}
};

// 主聊天页面程序
void MainMenu(int clientfd)
{
    help();

    char buffer[1024] = {0};
    while (is_main_menu_running)
    {
        std::cin.getline(buffer, 1024);
        std::string commandbuf(buffer);
        std::string command; // 存储命令
        int idx = commandbuf.find(":");
        if (-1 == idx)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, idx);
        }
        auto it = command_handler_map.find(command);
        if (it == command_handler_map.end())
        {
            std::cerr << "invalid input command!" << std::endl;
            continue;
        }

        // 调用相应命令的事件处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx)); // 调用命令处理方法
    }
}

// "help" command handler
void help(int, std::string)
{
    std::cout << "show command list >>> " << std::endl;
    for (auto &p : command_map)
    {
        std::cout << p.first << " : " << p.second << std::endl;
    }
    std::cout << std::endl;
}
// "addfriend" command handler
void addfriend(int clientfd, std::string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = current_user.GetId();
    js["friendid"] = friendid;
    std::string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        std::cerr << "send addfriend msg error -> " << buffer << std::endl;
    }
}
// "chat" command handler
void chat(int clientfd, std::string str)
{
    int idx = str.find(":"); // friendid:message
    if (-1 == idx)
    {
        std::cerr << "chat command invalid!" << std::endl;
        return;
    }

    int friendid = atoi(str.substr(0, idx).c_str());
    std::string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = current_user.GetId();
    js["name"] = current_user.GetName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = GetCurrentTime();
    std::string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        std::cerr << "send chat msg error -> " << buffer << std::endl;
    }
}
// "creategroup" command handler  groupname:groupdesc
void creategroup(int clientfd, std::string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        std::cerr << "creategroup command invalid!" << std::endl;
        return;
    }

    std::string groupname = str.substr(0, idx);
    std::string groupdesc = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = current_user.GetId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    std::string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        std::cerr << "send creategroup msg error -> " << buffer << std::endl;
    }
}
// "addgroup" command handler
void addgroup(int clientfd, std::string str)
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = current_user.GetId();
    js["groupid"] = groupid;
    std::string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        std::cerr << "send addgroup msg error -> " << buffer << std::endl;
    }
}
// "groupchat" command handler   groupid:message
void groupchat(int clientfd, std::string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        std::cerr << "groupchat command invalid!" << std::endl;
        return;
    }

    int groupid = atoi(str.substr(0, idx).c_str());
    std::string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = current_user.GetId();
    js["name"] = current_user.GetName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = GetCurrentTime();
    std::string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        std::cerr << "send groupchat msg error -> " << buffer << std::endl;
    }
}
// "loginout" command handler
void loginout(int clientfd, std::string)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = current_user.GetId();
    std::string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        std::cerr << "send loginout msg error -> " << buffer << std::endl;
    }
    else
    {
        is_main_menu_running = false;
    }   
}

// 获取系统时间（聊天信息需要添加时间信息）
std::string GetCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}