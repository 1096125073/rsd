//
// Created by xia on 2022/4/15.
//

#ifndef SERVER_H
#define SERVER_H

#include "common.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "Server"
#endif

typedef enum STATUS
{
    OFFLINE,SINGLE,COUPLE,RUNNING
}STATUS;


typedef struct Entry
{
    int status;

    char username[BUF_SIZE];
    char password[BUF_SIZE];

    int first_fd;
    int second_fd;
    pid_t pid;
} Entry;


#ifndef MAX_PAIRS
#define MAX_PAIRS 10
#endif

Entry entries[MAX_PAIRS];

void clearEntry(int pos);
void init();
int findEntryByName(const char *name);
int findEntryByPid(pid_t pid);
int findUnusedEntry();
void updateOneEntry(int pos);
void updateAllEntry();
int insertEntry(const char *name,const char *password,int fd);
void clearEntryById(pid_t pid);
void sigChildHandler(int sig);
int createServerSocket(const char *ip,int port);
void transmit(int in_fd,int out_fd);
void handleEntry(int pos);











#endif //SERVER_H
