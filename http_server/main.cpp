#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

#include "http_connection.h"
#include <Windows.h>

#include <fstream>
#include "..\\dbstruct.h"

void httpServer(tcp::acceptor& acceptor, tcp::socket& socket);
void readFile(std::ifstream& file, DBStruct& dbstruct, unsigned short& port);

int main(int argc, char* argv[])
{
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);

	try
	{
		DBStruct dbstruct;
		unsigned short port;

		std::ifstream file("..\\ini.txt");
		if (file.is_open()) {
			readFile(file, dbstruct, port);
		} else {
			throw std::exception("File ini.txt missing");
		}
		file.close();

		auto const address = net::ip::make_address("0.0.0.0");

		net::io_context ioc{1};

		tcp::acceptor acceptor{ioc, { address, port }};
		tcp::socket socket{ioc};
		httpServer(acceptor, socket);

		std::cout << "Open browser and connect to http://localhost:8080 to see the web server operating" << std::endl;

		ioc.run();
	}
	catch (std::exception const& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}

void httpServer(tcp::acceptor& acceptor, tcp::socket& socket) {
	acceptor.async_accept(socket, [&](beast::error_code ec) {
		if (!ec)
			std::make_shared<HttpConnection>(std::move(socket))->start();
		httpServer(acceptor, socket);
	});
}

void readFile(std::ifstream& file, DBStruct& dbstruct, unsigned short& port) {
	while (!file.eof()) {
		std::string input;
		file >> input;
		if (input == "host:") {
			file >> dbstruct.db_host;		
		} else if (input == "port:") {
			file >> dbstruct.db_port;		
		} else if (input == "dbname:") {
			file >> dbstruct.db_name;
		} else if (input == "user:") {
			file >> dbstruct.db_user;
		} else if (input == "password:") {
			file >> dbstruct.db_password;
		} else if (input == "port_search:") {
			file >> port;
		}
	}
}
