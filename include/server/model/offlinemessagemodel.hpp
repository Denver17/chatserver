#ifndef OFFLINEMESSAGEMODEL_H
#define OFFLINEMESSAGEMODEL_H

#include <string>
#include <vector>

// 提供离线消息表的操作接口
class OfflineMsgModel {
public:
  void insert(int user_id, std::string msg);

  void remove(int user_id);

  std::vector<std::string> query(int user_id);
};

#endif