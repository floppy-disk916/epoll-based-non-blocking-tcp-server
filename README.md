# Epoll-Based-Non-Blocking-TCP-Server
Implementation of asynchronous programming using linux event polling syscall and non-blocking TCP socket server written in C

This is a repository holding a source code which shows an implementation of a non-blocking TCP socket server combined with low level asynchronous programming using Linux event polling mechanism

This server only echoes back the received message to the sending client
## Build & Run
```
gcc epoll_tcp_server.c -o <the_program_name>
./<the_program_name>
```
The <the_program_name> can be anything you want
