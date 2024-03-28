#pragma once
#include <string>

struct DBStruct {
	std::string db_host;
	unsigned short db_port;
	std::string db_name;
	std::string db_user;
	std::string db_password;
};