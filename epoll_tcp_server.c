#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>

#define MAX_CONN 1000

typedef struct addrinfo addrinfo;
typedef struct epoll_event epoll_event;

int main(int argc, char* argv[]){
        if(argc != 3){
                printf("Usage: epoll_serv2 <IP_ADDR> <PORT>\n");
                return 0;
        }
        epoll_event reg_event, *revents;
        addrinfo hints, *serv;
        int epoll_fd, server_fd, client_fds[MAX_CONN], ready_fd, bytes, errcode;
        int exit_loop = 0;
        int client_counter = 0;
        int flag = 0;
        memset(&hints, 0, sizeof(addrinfo));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = 0;
        hints.ai_flags = 0;
        printf("[SYSTEM] Starting the server\n");
        if((errcode = getaddrinfo(argv[1], argv[2], &hints, &serv)) != 0){
                perror("getaddrinfo error");
                return 0;
        }
        if((server_fd = socket(serv->ai_family, serv->ai_socktype, serv->ai_protocol)) == -1){
                perror("socket error");
                freeaddrinfo(serv);
                return 0;
        }
        if((errcode = bind(server_fd, serv->ai_addr, serv->ai_addrlen)) == -1){
                perror("socket error");
                freeaddrinfo(serv);
                close(server_fd);
                return 0;
        }
        if((errcode = listen(server_fd, SOMAXCONN)) == -1){
                perror("listen error");
                freeaddrinfo(serv);
                close(server_fd);
                return 0;
        }
        socklen_t opt = 1;
        if((errcode = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, opt)))
        if((revents = calloc(MAX_CONN, sizeof(epoll_event))) == NULL){
                perror("allocating *revents error");
                freeaddrinfo(serv);
                close(server_fd);
                return 0;
        }
        if((epoll_fd = epoll_create(1)) == -1){
                perror("epoll create error");
                freeaddrinfo(serv);
                free(revents);
                close(server_fd);
                return 0;
        }

        if((flag = fcntl(server_fd, F_GETFL, 0)) == -1){
                perror("fcntl getfl error");
                freeaddrinfo(serv);
                free(revents);
                close(server_fd);
                return 0;
        }
        flag |= O_NONBLOCK;
        if((errcode = fcntl(server_fd, F_SETFL, flag)) == -1){
                perror("fcntl setfl error");
                freeaddrinfo(serv);
                free(revents);
                close(server_fd);
                return 0;
        }

        reg_event.data.fd = server_fd;
        reg_event.events = EPOLLIN|EPOLLET;
        if((errcode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &reg_event)) == -1){
                perror("epoll ctl add server fd error");
                freeaddrinfo(serv);
                free(revents);
                close(server_fd);
                return 0;
        }
        for(;;){
                if((ready_fd = epoll_wait(epoll_fd, revents, MAX_CONN, -1)) == -1){
                        perror("Epoll wait error");
                        continue;
                }
                for(int i = 0; i < ready_fd; i++){
                        if(revents[i].events & EPOLLHUP ||
                         revents[i].events & EPOLLERR){
                                fprintf(stderr, "epoll error\n");
                                close(revents[i].data.fd);
                                continue;
                        } else if (server_fd == revents[i].data.fd && revents[i].events & EPOLLIN){
                                if(client_counter >= SOMAXCONN){
                                        printf("The connection has reaches its max\n");
                                        continue;
                                }
                                if((client_fds[client_counter] =
                                accept(server_fd, serv->ai_addr, &serv->ai_addrlen)) == -1){
                                        perror("server accept error");
                                        continue;
                                }

                                if((flag = fcntl(client_fds[client_counter], F_GETFL)) == -1){
                                        perror("error at getting the client flag");
                                        close(client_fds[client_counter]);
                                        continue;
                                }
                                flag |= O_NONBLOCK;
                                if((errcode = fcntl(client_fds[client_counter], F_SETFL, flag)) == -1){
                                        perror("error at setting the client flag");
                                        close(client_fds[client_counter]);
                                        continue;
                                }

                                reg_event.data.fd = client_fds[client_counter];
                                reg_event.events = EPOLLIN;
                                if((errcode = epoll_ctl(epoll_fd,
                                EPOLL_CTL_ADD, client_fds[client_counter], &reg_event)) == -1){
                                        perror("client epoll ctl error");
                                        continue;
                                }
                                char wmessage[100] = "[SYSTEM] You've been connected to the server...\nYou can enter any message from now on...\n";
                                if((bytes = send(client_fds[client_counter],
                                wmessage, strlen(wmessage), 0)) == -1){
                                        perror("error sending welcome message...");
                                }
                                printf("[SYSTEM] Client %d has been registered\n", client_fds[client_counter]);
                                client_counter++;
                        } else {
                                char msg[1024];
                                char sendmsg[2000];
                                memset(msg, 0, sizeof(msg));
                                for(int j = 0; j < client_counter; j++){
                                        if(client_fds[j] == revents[i].data.fd && (revents[i].events & EPOLLIN
                                        || revents[i].events & EPOLLOUT)){
                                                bytes = recv(revents[i].data.fd, msg, sizeof(msg), 0);
                                                if(bytes == 0 || strcmp(msg, "\n") == 0 ||
                                                strcmp(msg, "-t\n") == 0){
                                                        printf("Terminate server...\n");
                                                        char *close_msg = "[SYSTEM] Terminating the connection. Please press enter after this...\n";
                                                        send(revents[i].data.fd, close_msg, strlen(close_msg), 0);
                                                        exit_loop = 1;
                                                        break;
                                                }
                                                if(bytes == -1){
                                                        perror("error on receiving data");
                                                        continue;
                                                }
                                                sprintf(sendmsg, "Client %d says: %s\n",
                                                revents[i].data.fd, msg);
                                                printf("[CLIENT] %s\n", sendmsg);
                                                if((bytes = send(revents[i].data.fd, sendmsg, strlen(sendmsg), 0))
                                                 == -1){
                                                        perror("error on echoing back message");
                                                        char* errmsg = "error on echoing back message\n";
                                                        send(revents[i].data.fd, errmsg, strlen(errmsg), 0);
                                                        continue;
                                                }
                                        }
                                }
                                if(exit_loop == 1){
                                        printf("Ending the loop...\n");
                                        break;
                                }
                        }
                }
                if(exit_loop == 1){
                        break;
                }
        }
        printf("[SYSTEM] Freeing the revents...\n");
        free(revents);
        printf("[SYSTEM] Cleaning up the resources...\n");
        for(int i = 0; i < client_counter; i++){
                if(close(client_fds[i]) == -1){
                        perror("error at closing client fds");  //watch our for other fd for their cleanup
                        return 0;
                }
        }
        printf("[SYSTEM] Cleaning client fds array has been finished...\n");
        freeaddrinfo(serv);
        close(server_fd);
        close(epoll_fd);
        return 0;
}
