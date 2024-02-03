#include "offlinemessagemodel.hpp"
#include "db.hpp"

void OfflineMsgModel::insert(int user_id, std::string msg) {
  char sql[1024] = {0};
  sprintf(sql, "insert into offlinemessage values('%d', '%s')", user_id, msg.c_str());

  MySQL mysql;
  if (mysql.connect()) {
    mysql.update(sql);
  }
}

void OfflineMsgModel::remove(int user_id) {
  char sql[1024] = {0};
  sprintf(sql, "delete from offlinemessage where userid='%d'", user_id);

  MySQL mysql;
  if (mysql.connect()) {
    mysql.update(sql);
  }
}

std::vector<std::string> OfflineMsgModel::query(int user_id) {
  char sql[1024] = {0};
  sprintf(sql, "select message from offlinemessage where userid='%d'", user_id);

  MySQL mysql;
  std::vector<std::string> vec;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql);
    if (res != nullptr) {
      MYSQL_ROW row;
      while ((row = mysql_fetch_row(res)) != nullptr) {
        vec.push_back(row[0]);
      }
      mysql_free_result(res);
      return vec;
    }
  }
  return {};
}