//
// Created by xia on 2022/4/19.
//
#include "common.h"
#include "sys/inotify.h"
#include "sys/stat.h"
#include "limits.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "Client"
#endif

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

Boolean CouldSynchronous = TRUE;
int createClientSocket(const char *ip,int port);
void handleCloseWrite(struct inotify_event *event,int client_fd);
void handleDelete(struct inotify_event *event,int client_fd);
void handleDeleteSelf(struct inotify_event *event,int client_fd);
void handleRename(struct inotify_event *event,int client_fd);
void disparityEvent(struct inotify_event *event,int client_fd);
int addInotifyDirWatch(int inotifyFd,const char *dir,uint32_t mask);
void handleInotifyEvent(int inotifyFd,int client_fd);
void dropData(int fd);
void handleTcpUpdate(int client_fd,const char *filename,const char* size);
void handleTcpDelete(const char *filename);
void handleTcpRename(const char *old,const char *new);
void handleTcpEvent(int client_fd);