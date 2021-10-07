#include <string>
#include <map>
#include "yhttpparser.h"
#include <sys/epoll.h>

typedef Response (*method_handler_ptr)(Request &request); //函数指针

class HttpServer
{
private:
    std::map<std::string, method_handler_ptr> resource_map;

public:
    int start(int port, int backlog = 10);

    void add_mapping(std::string path, method_handler_ptr handler);

    int close_and_remove_epoll_events(int &epollfd, epoll_event &epoll_event, int &fd);
};