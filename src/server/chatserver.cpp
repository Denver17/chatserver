#include "chatserver.hpp"
#include "chatservice.hpp"
#include <boost/bind.hpp>
#include "json.hpp"
#include <string>
// #include <functional>

using json = nlohmann::json;

ChatServer::ChatServer(EventLoop* loop,
              const InetAddress& listenAddr,
              const string& nameArg)
              : server_(loop, listenAddr, nameArg)
              , loop_(loop) {
  server_.setConnectionCallback(boost::bind(&ChatServer::onConnection, this, boost::placeholders::_1));
  server_.setMessageCallback(boost::bind(&ChatServer::onMessage, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
  server_.setThreadNum(4);
}

void ChatServer::start() {
  server_.start();
}

void ChatServer::onConnection(const TcpConnectionPtr& conn) {
  // 客户端断开连接
  if (!conn->connected()) {
    ChatService::instance()->ClientCloseException(conn);
    conn->shutdown();
  }
}

void ChatServer::onMessage(const TcpConnectionPtr& conn, Buffer* buffer, Timestamp time) {
  string buf = buffer->retrieveAllAsString();
  // 数据反序列化
  json js = json::parse(buf);
  // 解耦网络模块代码和业务模块代码
  auto msg_handler = ChatService::instance()->GetHandler(js["msgid"].get<int>());
  msg_handler(conn, js, time);
}