#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>
#include <iostream>

using namespace muduo;

ChatService* ChatService::instance() {
  static ChatService service;
  return &service;
}

ChatService::ChatService() {
  msg_handler_map_.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
  msg_handler_map_.insert({LOGINOUT_MSG, std::bind(&ChatService::LoginOut, this, _1, _2, _3)});
  msg_handler_map_.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
  msg_handler_map_.insert({ONE_CHAT_MSG, std::bind(&ChatService::OneChat, this, _1, _2, _3)});
  msg_handler_map_.insert({ADD_FRIEND_MSG, std::bind(&ChatService::AddFriend, this, _1, _2, _3)});

  msg_handler_map_.insert({CREATE_GROUP_MSG, std::bind(&ChatService::CreateGroup, this, _1, _2, _3)});
  msg_handler_map_.insert({ADD_GROUP_MSG, std::bind(&ChatService::AddGroup, this, _1, _2, _3)});
  msg_handler_map_.insert({GROUP_CHAT_MSG, std::bind(&ChatService::GroupChat, this, _1, _2, _3)});

  if (redis_.connect()) {
    redis_.init_notify_handler(std::bind(&ChatService::HandleRedisSubscribeMessage, this, _1, _2));
  }
}

void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time) {
  int id = js["id"].get<int>();
  std::string pwd = js["password"];

  User user = user_model_.Query(id);
  json response;
  response["msgid"] = LOGIN_MSG_ACK;
  if (user.GetId() == id && user.GetPwd() == pwd) {
    // 记录用户连接信息
    {
      std::lock_guard<std::mutex> lock(conn_mutex_);
      user_conn_map_.insert({id, conn});
    }

    // id用户登录成功后，向redis订阅channel(id)
    redis_.subscribe(id);

    // 更新用户状态
    user.SetState("online");
    user_model_.UpdateState(user);

    response["errno"] = 0;
    response["id"] = user.GetId();
    response["name"] = user.GetName();

    // 查询该用户是否有离线消息
    std::vector<std::string> vec = offline_msg_model_.query(id);
    if (!vec.empty()) {
      response["offlinemsg"] = vec;
      offline_msg_model_.remove(id);
    }

    // 查询该用户的好友信息
    std::vector<User> user_vec = friend_model_.query(id);
    if (!user_vec.empty()) {
      std::vector<std::string> temp;
      for (User &user : user_vec) {
        json temp_js;
        temp_js["id"] = user.GetId();
        temp_js["name"] = user.GetName();
        temp_js["state"] = user.GetState();
        temp.push_back(temp_js.dump());
      }
      response["friends"] = temp;
    }

    // 查询用户的群组信息
    vector<Group> group_user_vec = group_model_.QueryGroups(id);
    if (!group_user_vec.empty())
    {
        // group:[{groupid:[xxx, xxx, xxx, xxx]}]
        vector<string> groupV;
        for (Group &group : group_user_vec)
        {
            json grp_json;
            grp_json["id"] = group.GetId();
            grp_json["groupname"] = group.GetName();
            grp_json["groupdesc"] = group.GetDesc();
            vector<string> userV;
            for (GroupUser &user : group.GetUsers())
            {
                json js;
                js["id"] = user.GetId();
                js["name"] = user.GetName();
                js["state"] = user.GetState();
                js["role"] = user.GetRole();
                userV.push_back(js.dump());
            }
            grp_json["users"] = userV;
            groupV.push_back(grp_json.dump());
        }

        response["groups"] = groupV;
    }

  } else {
    response["errno"] = 1;
    response["errmsg"] = "name or password is invalid!";
  }
  conn->send(response.dump());
}

void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time) {
  std::string name = js["name"];
  std::string pwd = js["password"];

  User user;
  user.SetName(name);
  user.SetPwd(pwd);
  bool state = user_model_.Insert(user);
  json response;
  response["msgid"] = REG_MSG_ACK;
  if (state) {
    // 注册成功
    response["errno"] = 0;
    response["id"] = user.GetId();
  } else {
    // 注册失败
    response["errno"] = 1;
  }
  conn->send(response.dump());
}

void ChatService::OneChat(const TcpConnectionPtr &conn, json &js, Timestamp time) {
  int to_id = js["toid"].get<int>();
  {
    // 检查toid是否在线
    std::lock_guard<std::mutex> lock(conn_mutex_);
    auto iter = user_conn_map_.find(to_id);
    if (iter != user_conn_map_.end()) {
      // toid在线
      iter->second->send(js.dump());
      return;
    }
  }

  // 查询toid是否在其他服务器
  User user = user_model_.Query(to_id);
  if (user.GetState() == "online") {
    redis_.publish(to_id, js.dump());
    return;
  }

  // toid不在线，存储离线消息
  offline_msg_model_.insert(to_id, js.dump());
}

// 添加好友 msgid id friendid
void ChatService::AddFriend(const TcpConnectionPtr &conn, json &js, Timestamp time) {
  int user_id = js["id"].get<int>();
  int friend_id = js["friendid"].get<int>();

  friend_model_.insert(user_id, friend_id);
}

void ChatService::CreateGroup(const TcpConnectionPtr &conn, json &js, Timestamp time) {
  int user_id = js["id"].get<int>();
  string name = js["groupname"];
  string desc = js["groupdesc"];

  // 存储新创建的群组信息
  Group group(-1, name, desc);
  if (group_model_.CreateGroup(group))
  {
      // 存储群组创建人信息
      group_model_.AddGroup(user_id, group.GetId(), "creator");
  }
}

void ChatService::AddGroup(const TcpConnectionPtr &conn, json &js, Timestamp time) {
  int user_id = js["id"].get<int>();
  int group_id = js["groupid"].get<int>();
  group_model_.AddGroup(user_id, group_id, "normal");
}

void ChatService::GroupChat(const TcpConnectionPtr &conn, json &js, Timestamp time) {
  int user_id = js["id"].get<int>();
  int group_id = js["groupid"].get<int>();
  vector<int> user_id_vec = group_model_.QueryGroupUsers(user_id, group_id);

  lock_guard<mutex> lock(conn_mutex_);
  for (int id : user_id_vec)
  {
    auto it = user_conn_map_.find(id);
    if (it != user_conn_map_.end())
    {
      // 转发群消息
      it->second->send(js.dump());
    }
    else
    {
      // 查询id是否在其他服务器
      User user = user_model_.Query(id);
      if (user.GetState() == "online") {
        redis_.publish(id, js.dump());
        return;
      }
      offline_msg_model_.insert(id, js.dump());
    }
  }
}

MsgHandler ChatService::GetHandler(int msgid) {
  auto it = msg_handler_map_.find(msgid);
  // 没有对应的msgid, 返回默认handler
  if (it == msg_handler_map_.end()) {
    return [=](const TcpConnectionPtr &conn, json &js, Timestamp) {
      LOG_ERROR << "msgid: " << msgid << " can not find handler!";
    };
  }
  return msg_handler_map_[msgid];
}

void ChatService::ReSet() {
  // 将所有用户的状态设置为offline
  user_model_.ResetState();
}

void ChatService::LoginOut(const TcpConnectionPtr &conn, json &js, Timestamp time) {
  int user_id = js["id"].get<int>();

  {
      lock_guard<mutex> lock(conn_mutex_);
      auto it = user_conn_map_.find(user_id);
      if (it != user_conn_map_.end())
      {
          user_conn_map_.erase(it);
      }
  }

  // 用户注销，相当于就是下线，在redis中取消订阅通道
  redis_.unsubscribe(user_id); 

  // 更新用户的状态信息
  User user(user_id, "", "", "offline");
  user_model_.UpdateState(user);
}
 
void ChatService::ClientCloseException(const TcpConnectionPtr &conn) {
  User user;
  {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    // 从map表中删除用户信息
    for (auto iter = user_conn_map_.begin(); iter != user_conn_map_.end(); ++iter) {
      if (iter->second == conn) {
        user.SetId(iter->first);
        user_conn_map_.erase(iter);
        break;
      }
    }
  }
  // 更新用户状态
  if (user.GetId() != -1) {
    user.SetState("offline");
    user_model_.UpdateState(user);
  }
}

// 从redis消息队列中获取订阅的消息
void ChatService::HandleRedisSubscribeMessage(int user_id, std::string msg) {
  std::lock_guard<std::mutex> lock(conn_mutex_);
  auto it = user_conn_map_.find(user_id);
  if (it != user_conn_map_.end())
  {
      it->second->send(msg);
      return;
  }

  // 存储该用户的离线消息
  offline_msg_model_.insert(user_id, msg);
}