#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include "yhttpserver.h"
#include <unistd.h>

#include <fcntl.h>

Response res(200, "hello");

int HttpServer::start(int port, int backlog)
{

    int sockfd, new_fd, epfd;
    int flag;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }
    // int oldSocketFlag = fcntl(sockfd, F_GETFL, 0);
    // int newSocketFlag = oldSocketFlag | O_NONBLOCK;
    // if (fcntl(sockfd, F_SETFL, newSocketFlag) == -1)
    // {
    //     close(sockfd);
    //     std::cout << "set listenfd to nonblock error" << std::endl;
    //     return -1;
    // }
    struct epoll_event ev, events[20];
    epfd = epoll_create(256);
    struct sockaddr_in my_addr; /* my address information */
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;         /* host byte order */
    my_addr.sin_port = htons(port);       /* short, network byte order */
    my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */

    ev.data.fd = sockfd;
    ev.events = EPOLLIN | EPOLLET;
    flag = epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);
    if (flag < 0)
    {
        perror("epoll_ctl error!\n");
        return -1;
    }

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("bind");
        exit(1);
    }

    if (listen(sockfd, backlog) == -1)
    {
        perror("listen");
        exit(1);
    }
    // std::cout << "进来了吗1" << std::endl;
    while (true)
    {
        struct sockaddr_in their_addr; /* connector's address information */
        socklen_t sin_size = sizeof(struct sockaddr_in);
        int nfds = epoll_wait(epfd, events, 10, -1);
        // std::cout << "进来了吗" << std::endl;
        // std::cout << "nfds" << nfds << std::endl;
        for (size_t i = 0; i < nfds; ++i)
        {
            // std::cout << i << std::endl;
            // std::cout << "fuck" << std::endl;
            if ((events[i].data.fd == sockfd) && (events[i].events & EPOLLIN))
            {
                std::cout << "建立连接事件" << std::endl;
                new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
                // std::cout << new_fd << std::endl;
                if (new_fd < 0)
                {
                    perror("accept");
                    continue;
                }
                int oldSocketFlag = fcntl(sockfd, F_GETFL, 0);
                int newSocketFlag = oldSocketFlag | O_NONBLOCK;
                if (fcntl(sockfd, F_SETFL, newSocketFlag) == -1)
                {
                    close(sockfd);
                    std::cout << "set listenfd to nonblock error" << std::endl;
                    return -1;
                }
                std::cout << "server: got connection from " << inet_ntoa(their_addr.sin_addr) << std::endl;
                ev.data.fd = new_fd;
                // ev.events = EPOLLIN;
                ev.events = EPOLLIN;
                ev.events |= EPOLLET;
                int ctl = epoll_ctl(epfd, EPOLL_CTL_ADD, new_fd, &ev);
                if (ctl < 0)
                {
                    perror("epoll_ctl error!\n");
                    continue;
                }
            }
            else if (events[i].events & EPOLLIN)
            {
                std::cout << "EPOLL IN 读事件" << std::endl;
                int buffer_size = 1024;
                char read_buffer[buffer_size];
                memset(read_buffer, 0, buffer_size);

                int read_size;
                int line_num = 0;
                RequestLine req_line;
                if ((read_size = recv(new_fd, read_buffer, buffer_size, MSG_DONTWAIT)) > 0)
                {
                    if (read_buffer[read_size - 2] != '\r' || read_buffer[read_size - 1] != '\n')
                    {
                        // LOG_DEBUG("NOT VALID DATA!");
                        break;
                    }
                    std::string req_str(read_buffer, buffer_size);
                    // std::cout << "read from client: size:" << read_size << ",content:" << req_str.c_str() << std::endl;

                    std::stringstream ss(req_str);
                    std::string line;
                    int ret = 0;
                    while (ss.good() && line != "\r")
                    {
                        std::getline(ss, line, '\n');
                        line_num++;

                        // parse request line like  "GET /index.jsp HTTP/1.1"
                        if (line_num == 1)
                        {
                            ret = parse_request_line(line.c_str(), line.size() - 1, req_line);
                            if (ret == 0)
                            {
                                // LOG_DEBUG("parse_request_line success which method:%s, url:%s, http_version:%s", req_line.method.c_str(), req_line.request_url.c_str(), req_line.http_version.c_str());
                                // std::cout << "parse_request_line success which method:" << req_line.method.c_str() << ", url:" << req_line.request_url.c_str() << ", http_version:" << req_line.http_version.c_str() << std::endl;
                            }
                            else
                            {
                                // LOG_INFO("parse request line error!");
                                break;
                            }
                        }
                    }

                    if (ret != 0)
                    {
                        break;
                    }
                    if (line == "\r" && req_line.method == "GET")
                    {
                        Request req;
                        req.request_line = req_line;

                        // default http response

                        method_handler_ptr handle = this->resource_map[req.get_request_uri()];
                        if (handle != NULL)
                        {
                            res = handle(req);
                        }
                        std::cout << res.status_code << std::endl;
                        line_num = 0;
                    }
                    memset(read_buffer, 0, buffer_size);
                    ev.data.fd = new_fd;
                    ev.events = EPOLLOUT | EPOLLET;
                    std::cout << "读事件结束" << std::endl;
                    int ctl = epoll_ctl(epfd, EPOLL_CTL_MOD, new_fd, &ev);
                    if (ctl < 0)
                    {
                        perror("ctl error");
                    }
                }
                else
                {
                    close_and_remove_epoll_events(epfd, events[i], new_fd);
                }
                // std::cout << "读事件结束" << std::endl;
                // close_and_remove_epoll_events(epfd, events[i], new_fd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                std::cout << "EPOLL OUT 写事件" << std::endl;
                std::string res_content = res.gen_response();
                int one_write = send(new_fd, res_content.c_str(), res_content.size(), 0);
                if(one_write < 0) {
					perror("send");
				}
                
                events[i].events = EPOLLIN | EPOLLOUT | EPOLLET;
                epoll_ctl(epfd, EPOLL_CTL_DEL, new_fd, &events[i]);
                int ret = close(new_fd);
                std::cout << "写事件结束2" << "connect close complete which fd, ret:  "<< new_fd << " " << ret <<std::endl;
                std::cout << "    " << std::endl;
                // std::cout << "写事件结束" << std::endl;

                // if (send(new_fd, res_content.c_str(), res_content.size(), 0) == -1)
                // {
                //     perror("send");
                // }
                // line_num = 0;
            }
        }
    }
    return 0;
}
void HttpServer::add_mapping(std::string path, method_handler_ptr handler)
{
    resource_map[path] = handler;
}

int HttpServer::close_and_remove_epoll_events(int &epollfd, epoll_event &epoll_event, int &fd)
{
    // LOG_INFO("connect close");
    // HttpContext *hc = (HttpContext *)epoll_event.data.ptr;
    // int fd = hc->fd;
    epoll_event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &epoll_event);

    if (epoll_event.data.ptr != NULL)
    {
        // delete (HttpContext *)epoll_event.data.ptr;
        epoll_event.data.ptr = NULL;
    }

    int ret = close(fd);
    // LOG_DEBUG("connect close complete which fd:%d, ret:%d", fd, ret);
    return 0;
}
// struct sockaddr_in their_addr; /* connector's address information */
// socklen_t sin_size = sizeof(struct sockaddr_in);

// if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1)
// {
//     perror("accept");
//     continue;
// }

// // LOG_DEBUG("server: got connection from %s\n", inet_ntoa(their_addr.sin_addr));

// int buffer_size = 1024;
// char read_buffer[buffer_size];
// memset(read_buffer, 0, buffer_size);

// int read_size;
// int line_num = 0;
// RequestLine req_line;
// while ((read_size = recv(new_fd, read_buffer, buffer_size, 0)) > 0)
// {

//     if (read_buffer[read_size - 2] != '\r' || read_buffer[read_size - 1] != '\n')
//     {
//         // LOG_DEBUG("NOT VALID DATA!");
//         break;
//     }
//     std::string req_str(read_buffer, buffer_size);
//     std::cout << "read from client: size:" << read_size << ",content:" << req_str.c_str() << std::endl;

//     std::stringstream ss(req_str);
//     std::string line;
//     int ret = 0;
//     while (ss.good() && line != "\r")
//     {
//         std::getline(ss, line, '\n');
//         line_num++;

//         // parse request line like  "GET /index.jsp HTTP/1.1"
//         if (line_num == 1)
//         {
//             ret = parse_request_line(line.c_str(), line.size() - 1, req_line);
//             if (ret == 0)
//             {
//                 // LOG_DEBUG("parse_request_line success which method:%s, url:%s, http_version:%s", req_line.method.c_str(), req_line.request_url.c_str(), req_line.http_version.c_str());
//                 std::cout << "parse_request_line success which method:" << req_line.method.c_str() << ", url:" << req_line.request_url.c_str() << ", http_version:" << req_line.http_version.c_str() << std::endl;
//             }
//             else
//             {
//                 // LOG_INFO("parse request line error!");
//                 break;
//             }
//         }
//     }

//     if (ret != 0)
//     {
//         break;
//     }

//     if (line == "\r" && req_line.method == "GET")
//     {
//         Request req;
//         req.request_line = req_line;

//         // default http response
//         Response res(200, "hello");

//         method_handler_ptr handle = this->resource_map[req.get_request_uri()];
//         if (handle != NULL)
//         {
//             res = handle(req);
//         }

//         std::string res_content = res.gen_response();
//         if (send(new_fd, res_content.c_str(), res_content.size(), 0) == -1)
//         {
//             perror("send");
//         }
//         line_num = 0;
//         // http 1.0 close socket by server, 1.1 close by client
//         if (req_line.http_version == "HTTP/1.0")
//         {
//             break;
//         }
//     }

//     memset(read_buffer, 0, buffer_size);
// }

// // LOG_DEBUG("connect close!");
// close(new_fd);