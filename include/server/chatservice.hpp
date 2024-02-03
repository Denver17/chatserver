#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <mutex>
#include "json.hpp"

#include "redis.hpp"
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"

using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp)>;

class ChatService {
public:
  // 获取单例对象的接口函数
  static ChatService* instance();

  void login(const TcpConnectionPtr &conn, json &js, Timestamp time);

  void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);

  // 一对一聊天
  void OneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
  
  // 添加好友
  void AddFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);

  void CreateGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);

  void AddGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);

  void GroupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);

  void LoginOut(const TcpConnectionPtr &conn, json &js, Timestamp time);

  // 获取消息对应的Handler
  MsgHandler GetHandler(int msgid);

  // 处理服务器异常
  void ReSet();

  // 处理客户端异常退出
  void ClientCloseException(const TcpConnectionPtr &conn);

  // 从redis消息队列中获取订阅的消息
  void HandleRedisSubscribeMessage(int, std::string);
private:
  ChatService();

  // 存储消息id与对应的handler
  std::unordered_map<int, MsgHandler> msg_handler_map_;

  // 存储在线用户的通信连接
  std::unordered_map<int, TcpConnectionPtr> user_conn_map_;

  // 定义互斥锁， 保证user_conn_map_的线程安全
  std::mutex conn_mutex_;

  // 数据操作类对象
  UserModel user_model_;

  // 离线消息操作类对象
  OfflineMsgModel offline_msg_model_;

  FriendModel friend_model_;

  GroupModel group_model_;

  Redis redis_;
};

#endif