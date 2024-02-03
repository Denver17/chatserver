#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"
#include <string>
#include <vector>
using namespace std;

// 维护群组信息的操作接口方法
class GroupModel {
public:
    // 创建群组
    bool CreateGroup(Group &group);
    // 加入群组
    void AddGroup(int user_id, int group_id, string role);
    // 查询用户所在群组信息
    vector<Group> QueryGroups(int user_id);
    // 根据指定的groupid查询群组用户id列表，除userid自己，主要用户群聊业务给群组其它成员群发消息
    vector<int> QueryGroupUsers(int user_id, int group_id);
};

#endif