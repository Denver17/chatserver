#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H

#include "user.hpp"
#include <vector>

class FriendModel {
public:
  // 添加好友
  void insert(int user_id, int friend_id);

  // 返回用户好友列表
  std::vector<User> query(int user_id);
};

#endif