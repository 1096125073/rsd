//
// Created by xia on 2022/4/15.
//

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "stdarg.h"
#include <netinet/in.h>
#include "netinet/tcp.h"
#include <time.h>
#include "errno.h"
#include "error_handlers.h"
#include "arpa/inet.h"
#include "sys/epoll.h"

#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif

#define LOG_TAG "LOG"
#define LOG_FMT(level, fmt, tag, fd, color,...) \
                                            fprintf(fd,("%s%s: [%-5s] [Time: %s %s] [Line: %d] [Func: %s] " \
                                            "[Message: " fmt "]\n"),color,tag, level, __DATE__,__TIME__,\
                                            __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) LOG_FMT("INFO", fmt, LOG_TAG, stdout, "\x1b[36m", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) LOG_FMT("WARN", fmt, LOG_TAG, stdout, "\x1b[33m", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG_FMT("ERROR", fmt, LOG_TAG, stderr, "\x1b[31m", ##__VA_ARGS__)


typedef enum Boolean
{
    FALSE=0,TRUE=1
}Boolean;
typedef enum CMD
{
    CONNECTING=0,UPDATE=1,DELETE=2,RENAME=3,DELETE_SELF=4,MESSAGE=5,PAIRED=6,UNPAIRED=7,RESPONSE=8
}CMD;

typedef enum ERRNO
{
    INSERT_ERRNO=400
}ERRNO;

static const char * STR_CMD[] = {
        "CONNECTING","UPDATE","DELETE","RENAME","DELETE_SELF","MESSAGE",
        "PAIRED","UNPAIRED","RESPONSE"
};

ssize_t readLine(int sock,char *buf,int size);
Boolean isSock(int fd);
void safeCloseSock(int fd);
int isConnected(int fd);
int setNonBlock(int fd);
int addEpollInLetEvent(int epoll_fd,int fd);
int addEpollInLevelEvent(int epoll_fd,int fd);
Boolean sendInfo(int fd,const char *header_type,const char *arg1,const char *arg2);
Boolean sendMsg(int fd,const char *format,...);
#endif //COMMON_H
