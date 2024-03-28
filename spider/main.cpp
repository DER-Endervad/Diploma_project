#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

#include "http_utils.h"
#include <functional>

#include <fstream>
#include "..\\dbstruct.h"
#include <boost/locale.hpp>

std::mutex mtx;
std::condition_variable cv;
std::queue<std::function<void()>> tasks;
bool exitThreadPool = false;

void threadPoolWorker();
void parseLink(const Link& link, int depth);
void readFile(std::ifstream& file, DBStruct& dbstruct, Link& link, int& rec_depth);
Link setLink(const std::string& input, const Link& link);
void cleanHTML(std::string& html, std::vector<Link>& links, const Link& link);
void savingCleanHTML(const std::string& html, int& count, std::string& newHTML);
void endHtmlTag(const std::string& html, int& count, const int& size, const char& val, const char& val2, const char& val3, const char& val4);

int main() {
	try {
		int numThreads = std::thread::hardware_concurrency();
		std::vector<std::thread> threadPool;

		for (int i = 0; i < numThreads; ++i) {
			threadPool.emplace_back(threadPoolWorker);
		}

		DBStruct dbstruct;
		Link link;
		int recursion_depth;

		std::ifstream file("..\\ini.txt");
		if (file.is_open()) {
			readFile(file, dbstruct, link, recursion_depth);
		} else {
			throw std::exception("File ini.txt missing");
		}
		file.close();

		{
			std::lock_guard<std::mutex> lock(mtx);
			tasks.push([link, recursion_depth]() { parseLink(link, recursion_depth); });
			cv.notify_one();
		}

		std::this_thread::sleep_for(std::chrono::seconds(2));

		{
			std::lock_guard<std::mutex> lock(mtx);
			exitThreadPool = true;
			cv.notify_all();
		}

		for (auto& t : threadPool) {
			t.join();
		}
	}
	catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
	}
	return 0;
}

void threadPoolWorker() {
	std::unique_lock<std::mutex> lock(mtx);
	while (!exitThreadPool || !tasks.empty()) {
		if (tasks.empty()) {
			cv.wait(lock);
		} else {
			auto task = tasks.front();
			tasks.pop();
			lock.unlock();
			task();
			lock.lock();
		}
	}
}

void parseLink(const Link& link, int depth) {
	try {

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		std::string html = getHtmlContent(link);

		if (html.size() == 0) {
			std::cout << "Failed to get HTML Content" << std::endl;
			return;
		}

		// TODO: Collect more links from HTML code and add them to the parser like that - —оберите больше ссылок из HTML-кода и добавьте их
		// в парсер следующим образом.

		std::vector<Link> links;

		// TODO: Parse HTML code here on your own - –азберите HTML-код здесь самосто€тельно

		cleanHTML(html, links, link);

		std::cout << "html content:" << std::endl;
		std::cout << html << std::endl;

		/*if (depth > 0) {
			std::lock_guard<std::mutex> lock(mtx);

			size_t count = links.size();
			size_t index = 0;
			for (auto& sublink : links)
			{
				tasks.push([sublink, depth]() { parseLink(sublink, depth - 1); });
			}
			cv.notify_one();
		}*/
	}
	catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
	}
}

void readFile(std::ifstream& file, DBStruct& dbstruct, Link& link, int& rec_depth) {
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
		} else if (input == "start_page:") {
			std::string input_link;
			file >> input_link;
			link = setLink(input_link, link);
		} else if (input == "recursion_depth:") {
			file >> rec_depth;
		}
	}
}

Link setLink(const std::string& input, const Link& link) {
	std::string protocolStr;
	std::string hostName;
	std::string query;
	int number;
	ProtocolType protocol;
	Link this_link;

	if (input[0] == 'h') {
		number = 0;
	} else if (input[0] == '/' && input[1] == '/') {
		protocol = link.protocol;
		number = 1;
	} else if (input[0] == '/' && input[1] != '/') {
		protocol = link.protocol;
		hostName = link.hostName;
		number = 2;
	} else {
		throw std::exception("Invalid link");
	}

	for (int i = 0; i < input.size(); i++) {
		if (number == 0 && input[i] != ':') {
			protocolStr = protocolStr + input[i];
		} else if (number == 0 && input[i] == ':') {
			number = 1;
		} else if ((number == 1 || number == 2) && input[i] != '/') {
			hostName = hostName + input[i];
			number = 2;
		} else if (number == 2 && input[i] == '/') {
			query = query + input[i];
			number = 3;
		} else if (number == 3) {
			query = query + input[i];
		}
	}

	if (protocolStr == "http") {
		protocol = {ProtocolType::HTTP};
	} else if (protocolStr == "https") {
		protocol = {ProtocolType::HTTPS};
	}

	this_link = {protocol, hostName, query};
	return this_link;
}

void cleanHTML(std::string& html, std::vector<Link>& links, const Link& link) {
	std::string linkStr;
	Link this_link;
	std::string newHTML = "";
	int sizeHTML = html.size();

	for (int i = 0; i < sizeHTML; ++i) {
		if (html[i] == '<') { // <a href=" || <>
			if (html[i + 1] == 'a' && html[i + 2] == ' ' && html[i + 3] == 'h' && html[i + 4] == 'r' && html[i + 5] == 'e' &&
				html[i + 6] == 'f' && html[i + 9] != '#') { // <a href="
				i = i + 9;
				for (i; i < sizeHTML; i++) {
					if (html[i] == '"') {
						this_link = setLink(linkStr, link);
						links.push_back(this_link);
						linkStr.clear();
						for (i; i < sizeHTML; i++) {
							if (html[i] == '>') {
								break;
							}
						}
						break;
					} else {
						linkStr = linkStr + html[i];
					}
				}
			} else if (html[i + 1] == 's' && html[i + 2] == 't' && html[i + 3] == 'y' && html[i + 4] == 'l' &&
					   html[i + 5] == 'e') { // <style> ... </style>
				endHtmlTag(html, i, sizeHTML, '<', '/', 's', 't');
				i = i + 7;
			} else if (html[i + 1] == 's' && html[i + 2] == 'c' && html[i + 3] == 'r') { // <script> ... </script>
				endHtmlTag(html, i, sizeHTML, '<', '/', 's', 'c');
				i = i + 8;
			} else if (html[i + 1] == 's' && html[i + 2] == 'p' && html[i + 3] == 'a') { // <span> ... </span>
				endHtmlTag(html, i, sizeHTML, '<', '/', 's', 'p');
				i = i + 6;
			} else { // <>
				int depthHTML = 0;
				for (i; i < sizeHTML; i++) {
					if (html[i] == '<') {
						depthHTML++;
					} else if (html[i] == '>') {
						depthHTML--;
						if (depthHTML == 0) {
							html[i] = ' ';
							i--;
							break;
						}
					}
				}
			}
		} else if (html[i] == '&' && html[i + 1] == '#' && html[i + 2] >= 48 && html[i + 2] <= 57) { // &#...;
			for (i; i < sizeHTML; i++) {
				if (html[i] == ';') {
					break;
				}
			}
			//searchChar(html, i, sizeHTML, ';');
		} else if (html[i] == '&' && html[i + 1] == 'g' && html[i + 2] == 't' && html[i + 3] == ';') { // &gt;
			i = i + 3;
		} else if (html[i] == '&' && html[i + 1] == 'a' && html[i + 2] == 'm' && html[i + 3] == 'p' && html[i + 4] == ';') { // &amp;
			i = i + 4;
		} else if (html[i] == 39 && html[i + 1] == 's') {
			savingCleanHTML(html, i, newHTML);
		} else if ((html[i] >= 33 && html[i] <= 47) || (html[i] >= 58 && html[i] <= 64) || (html[i] >= 91 && html[i] <= 96) ||
				   (html[i] >= 123 && html[i] <= 127) || html[i] >= 176) {
			if (newHTML.size() >= 1) {
				if (newHTML[newHTML.size() - 1] != ' ') {
					newHTML = newHTML + ' ';
				}
			}
		} else {
			savingCleanHTML(html, i, newHTML);
		}
	}

	// Create system default locale
	boost::locale::generator gen;
	std::locale loc = gen("");
	std::locale::global(loc);
	std::wcout.imbue(loc);

	html = boost::locale::to_lower(newHTML);
}

void savingCleanHTML(const std::string& html, int& count, std::string& newHTML) {
	if (html[count] != ' ' && html[count] != '\n' && html[count] != '\t') {
		newHTML = newHTML + html[count];
	} else if (newHTML.size() >= 1) {
		if (newHTML[newHTML.size() - 1] != ' ' && (html[count] == ' ' || html[count] == '\n' || html[count] == '\t')) {
			newHTML = newHTML + ' ';
		}
	}
}

void endHtmlTag(
	const std::string& html, int& count, const int& size, const char& val, const char& val2, const char& val3, const char& val4) {
	for (count; count < size; count++) {
		if (html[count] == val && html[count + 1] == val2 && html[count + 2] == val3 && html[count + 3] == val4) {
			return;
		}
	}
}
