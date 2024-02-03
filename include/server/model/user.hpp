#ifndef USER_H
#define USER_H

#include <string>

class User {
public:
  User(int id = -1, std::string name = "", std::string pwd = "", std::string state = "offline") {
    this->id_ = id;
    this->name_ = name;
    this->password_ = pwd;
    this->state_ = state;
  }

  void SetId(int id)  {this->id_ = id;}
  void SetName(std::string name)  {this->name_ = name;}
  void SetPwd(std::string pwd)  {this->password_ = pwd;}
  void SetState(std::string state)  {this->state_ = state;}

  int GetId()  {return id_;}
  std::string GetName()  {return name_;}
  std::string GetPwd()  {return password_;}
  std::string GetState()  {return state_;}
private:
  int id_;
  std::string name_;
  std::string password_;
  std::string state_;
};

#endif