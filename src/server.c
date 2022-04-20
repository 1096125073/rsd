//
// Created by xia on 2022/4/15.
//

#include "server.h"
#include "sys/wait.h"
#include "signal.h"


/*
 * clear an entry
 * */
void clearEntry(int pos)
{
    entries[pos].status = OFFLINE;
    entries[pos].pid = -1;
    safeCloseSock(entries[pos].first_fd);
    entries[pos].first_fd = -1;
    safeCloseSock(entries[pos].second_fd);
    entries[pos].second_fd = -1;
    entries[pos].username[0] = '\0';
    entries[pos].password[0] = '\0';
}

void init()
{
    LOG_INFO("init");
    for(int i=0;i<MAX_PAIRS;i++)
        clearEntry(i);
}

/*
 * find entry by name,if don't find,return NULL
 * */
int findEntryByName(const char *name)
{
    for(int i=0;i<MAX_PAIRS;i++)
    {
        if(strcmp(entries[i].username,name) == 0)
            return i;
    }
    return -1;
}

int findEntryByPid(pid_t pid)
{
    for(int i=0;i<MAX_PAIRS;i++)
    {
        if(entries[i].pid == pid)
            return i;
    }
    return -1;
}
/*
 * find unused entry,except RUNNING status
 * */
int findUnusedEntry()
{
    for(int i=0;i<MAX_PAIRS;i++)
    {
        if(entries[i].status == OFFLINE)
            return i;
    }
    for(int i=0;i<MAX_PAIRS;i++)
    {
        if(entries[i].status == SINGLE)
            return i;
    }
    for(int i=0;i<MAX_PAIRS;i++)
    {
        if(entries[i].status == COUPLE)
            return i;
    }
    return -1;
}

void updateOneEntry(int pos)
{
    int first = isConnected(entries[pos].first_fd);
    int second = isConnected(entries[pos].second_fd);
    if(first+ second == 2 && entries[pos].status == RUNNING)
        return;
    else if(first + second == 1)
    {
        if(second == 1)
        {
            entries[pos].first_fd = entries[pos].second_fd;
            entries[pos].second_fd = -1;
        }
        entries[pos].status = SINGLE;
        entries[pos].pid = -1;
        sendHeader(entries[pos].first_fd,STR_CMD[UNPAIRED],NULL,NULL);
        LOG_INFO("entry %d exist one established connection,name: %s",pos,entries[pos].username);
        return;
    }
    clearEntry(pos);
}
/*
 * update entry,if there are one established connection
 * update the status to single
 * */
void updateAllEntry()
{
    LOG_INFO("start updateEntry");
    for(int i=0;i<MAX_PAIRS;i++)
        updateOneEntry(i);
    LOG_INFO("end updateEntry");
}
/*
 * insert an entry by name and password
 * return -1 if insert fail
 * return the pos of entry
 * check the status of entry to determine the next step
 * */
int insertEntry(const char *name,const char *password,int fd)
{
    if(name == NULL || password == NULL)
        return -1;
    updateAllEntry();
    int pos = findEntryByName(name);
    if(pos==-1)
    {
        LOG_INFO("Couldn't find name: %s,try to find unused position",name);
        pos = findUnusedEntry();
        if(pos == -1)
        {
            LOG_WARN("Couldn't find an unused position to name: %s",name);
            return -1;
        }
        clearEntry(pos);
        strcpy(entries[pos].username,name);
        strcpy(entries[pos].password,password);
        entries[pos].status = SINGLE;
        entries[pos].first_fd = fd;
        entries[pos].second_fd = -1;
        return pos;
    }
    else if(entries[pos].status == SINGLE)
    {
        if(strcmp(entries[pos].password,password) == 0 )
        {
            entries[pos].status = COUPLE;
            entries[pos].second_fd = fd;
            return pos;
        } else
            LOG_WARN("password for name: %s is not correct",name);
    }
    return -1;
}
/*
 * clear the entry if subprocess exit
 * if not found the entry or the entry is using,return -1 else return 0
 * */
void clearEntryById(pid_t pid)
{
    int pos = findEntryByPid(pid);
    if(pos == -1)
    {
        LOG_WARN("Couldn't find entry for pid: %d",pid);
        return;
    }
    updateOneEntry(pos);
}
/*
 * when a subprocess exit,update the entry status
 * */
void sigChildHandler(int sig)
{
    pid_t pid;
    while ((pid = waitpid(-1,NULL,WNOHANG)) > 0)
        clearEntryById(pid);
}

int createServerSocket(const char *ip,int port)
{
    int sock;
    int reuse = 1;

    struct sockaddr_in server_address;
    memset(&server_address,0,sizeof(struct sockaddr_in));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    if(inet_pton(AF_INET,ip,&server_address.sin_addr) <= 0)
    {
        LOG_ERROR("ip: %s is not correct!",ip);
        return -1;
    }

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        LOG_ERROR("socket error!");
        return -1;
    }
    /* Address can be reused instantly after program exits */
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);
    /* Bind socket to server address */
    if(bind(sock,(struct sockaddr*) &server_address, sizeof(server_address)) < 0){
        LOG_ERROR("Cannot connect socket to address");
        return -1;
    }
    listen(sock,5);
    return sock;
}
/*
 * transmit the data between the pairs
 * */
void transmit(int in_fd,int out_fd)
{
    static char buf[BUF_SIZE];
    ssize_t numRead;
    while (TRUE)
    {
        numRead = read(in_fd,buf,BUF_SIZE);
        if(numRead == -1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else
                continue;
        }
        write(out_fd,buf,numRead);
    }
}

void handleEntry(int pos)
{
    int first_fd=entries[pos].first_fd,second_fd = entries[pos].second_fd;
    if(isConnected(first_fd) + isConnected(second_fd) !=2)
        err_exit("couple fd is not all open!");
    sendHeader(first_fd,STR_CMD[PAIRED],NULL,NULL);
    sendHeader(second_fd,STR_CMD[PAIRED],NULL,NULL);
    int epoll_fd;
    epoll_fd = epoll_create(2);
    if(epoll_fd == -1)
        err_exit("epoll_create");
    struct epoll_event events[2];
    if(setNonBlock(first_fd) == -1)
    {
        close(first_fd);
        err_exit("fcntl first.");
    }

    if(addEpollInLetEvent(epoll_fd,first_fd) == -1)
        err_exit("epoll_ctl first_fd.");

    if(setNonBlock(second_fd) == -1)
    {
        close(second_fd);
        err_exit("fcntl second_fd.");
    }

    if(addEpollInLetEvent(epoll_fd,second_fd) == -1)
        err_exit("epoll_ctl second_fd.");
    int eventRead,i;

    while (TRUE)
    {
        eventRead = epoll_wait(epoll_fd,events,2,-1);
        if(eventRead == -1)
        {
            if(errno == EINTR)
                continue;
            else
                err_exit("epoll_wait");
        }
        for (i = 0; i < eventRead; ++i) {
            if(events[i].events & EPOLLIN)
            {
                if(events[i].data.fd == first_fd)
                    transmit(first_fd,second_fd);

                else if(events[i].data.fd == second_fd)
                    transmit(second_fd,first_fd);
            }
            if(events[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP))
            {
                close(events[i].data.fd);
                errExit("connection was closed");
            }
        }
    }
}

int main(int argc,char *argv[])
{
    if(argc!=3 || strcmp(argv[1],"--help") == 0)
        usageErr("%s ip port\n",argv[0]);
    int port = (int)strtol(argv[2],NULL,10);
    signal(SIGCHLD,sigChildHandler);
    init();
    int server_fd = createServerSocket(argv[1],port);
    if(server_fd == -1)
        errExit("create_server_socket");
    struct sockaddr_in client_address;
    socklen_t socklen = sizeof(struct sockaddr_in);
    int client_fd;
    ssize_t numRead;
    pid_t pid;
    char *username,*password,*command;
    char buf[BUF_SIZE];
    LOG_INFO("start to receive connection");
    while (TRUE)
    {
        socklen = sizeof(struct sockaddr_in);
        client_fd = accept(server_fd,(struct sockaddr*)&client_address,&socklen);
        if(client_fd < 0)
            errExit("accept failed");
        if(inet_ntop(AF_INET,&client_address.sin_addr,buf,BUF_SIZE)!=NULL)
            LOG_INFO("Receive connection from (%s,%u)",buf, ntohs(client_address.sin_port));
        numRead = readLine(client_fd,buf,BUF_SIZE);
        if(numRead == 0)
        {
            LOG_WARN("readLine return zero");
            close(client_fd);
            continue;
        }
        if(numRead == BUF_SIZE - 1) //drop the too long data
        {
            LOG_WARN("header is too long,the data will");
            close(client_fd);
            continue;
        }

        command = strtok(buf," \n");
        if(strcmp(command,STR_CMD[CONNECTING]) != 0)
        {
            errMsg("Command is not right!");
            sendMsg(client_fd,INSERT_ERRNO,"%s","Command is not right!");
            close(client_fd);
            continue;
        }
        username = strtok(NULL," \n");
        password = strtok(NULL," \n");
        int pos = insertEntry(username,password,client_fd);
        if( pos == -1)
        {
            sendMsg(client_fd,INSERT_ERRNO,"%s","connect error,check the name and password");
            LOG_WARN("insert user: %s,password: %s failed",username,password);
            close(client_fd);
            continue;
        }
        LOG_INFO("user: %s,Status: %d",username,entries[pos].status);
        if(entries[pos].status == COUPLE)
        {
            pid = fork();
            if(pid < 0)
                errExit("fork");
            if(pid == 0)
            {
                close(server_fd);
                handleEntry(pos);
                _exit(0);
            }
            else
            {
                entries[pos].pid = pid;
                entries[pos].status = RUNNING;
                LOG_INFO("user: %s is Running,pid: %d",username,entries[pos].pid);
            }
        }
        else if(entries[pos].status == SINGLE)
        {
            sendHeader(client_fd,STR_CMD[MESSAGE],"Server: Connect success,waiting "
                                                  "the pair to connect",NULL);
            sendHeader(client_fd,STR_CMD[UNPAIRED],NULL,NULL);
        }

    }
}