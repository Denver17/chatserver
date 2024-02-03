#include "groupmodel.hpp"
#include "db.hpp"

// 创建群组
bool GroupModel::CreateGroup(Group &group)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into allgroup(groupname, groupdesc) values('%s', '%s')",
            group.GetName().c_str(), group.GetDesc().c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            group.SetId(mysql_insert_id(mysql.GetConnection()));
            return true;
        }
    }

    return false;
}

// 加入群组
void GroupModel::AddGroup(int user_id, int group_id, string role)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into groupuser values(%d, %d, '%s')",
            group_id, user_id, role.c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询用户所在群组信息
std::vector<Group> GroupModel::QueryGroups(int user_id)
{
    /*
    1. 先根据userid在groupuser表中查询出该用户所属的群组信息
    2. 在根据群组信息，查询属于该群组的所有用户的userid，并且和user表进行多表联合查询，查出用户的详细信息
    */
    char sql[1024] = {0};
    sprintf(sql, "select a.id,a.groupname,a.groupdesc from allgroup a inner join \
         groupuser b on a.id = b.groupid where b.userid=%d", user_id);

    std::vector<Group> group_vec;

    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            // 查出userid所有的群组信息
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                Group group;
                group.SetId(atoi(row[0]));
                group.SetName(row[1]);
                group.SetDesc(row[2]);
                group_vec.push_back(group);
            }
            mysql_free_result(res);
        }
    }

    // 查询群组的用户信息
    for (Group &group : group_vec)
    {
        sprintf(sql, "select a.id,a.name,a.state,b.grouprole from user a \
            inner join groupuser b on b.userid = a.id where b.groupid=%d",
                group.GetId());

        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                GroupUser user;
                user.SetId(atoi(row[0]));
                user.SetName(row[1]);
                user.SetState(row[2]);
                user.SetRole(row[3]);
                group.GetUsers().push_back(user);
            }
            mysql_free_result(res);
        }
    }
    return group_vec;
}

// 根据指定的groupid查询群组用户id列表，除userid自己，主要用户群聊业务给群组其它成员群发消息
std::vector<int> GroupModel::QueryGroupUsers(int user_id, int group_id)
{
    char sql[1024] = {0};
    sprintf(sql, "select userid from groupuser where groupid = %d and userid != %d", group_id, user_id);

    std::vector<int> id_vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                id_vec.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
    }
    return id_vec;
}