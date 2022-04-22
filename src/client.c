//
// Created by xia on 2022/4/13.
//

#include <sys/sendfile.h>
#include "client.h"


int createClientSocket(const char *ip,int port)
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
    if(connect(sock,(struct sockaddr*) &server_address, sizeof(server_address)) < 0){
        LOG_ERROR("Cannot connect socket to address");
        return -1;
    }
    return sock;
}

/*
 * when a file was created or changed,this function will be used.
 * */
void handleCloseWrite(struct inotify_event *event,int client_fd)
{
    static char buf[BUF_SIZE];
    if(event->len > 0)
    {
        int fd = open(event->name,O_RDONLY);
        if(fd == -1)
        {
            LOG_WARN("Could not open file: %s",event->name);
            return;
        }
        off_t size = lseek(fd,0,SEEK_END);
        if(size == (off_t)-1)
        {
            LOG_WARN("lseek failed: %s",event->name);
            return;
        }
        lseek(fd,0,SEEK_SET);
        snprintf(buf,BUF_SIZE,"%ld",(long int)size);
        ssize_t curSend,numSend = 0;
        int val = 1;
        setsockopt(client_fd,IPPROTO_TCP,TCP_CORK,&val,sizeof(val));
        clock_t start = clock();
        if(sendInfo(client_fd,STR_CMD[UPDATE],event->name,buf))
        {
            while (numSend < size)
            {
                curSend = sendfile(client_fd,fd,NULL,size-numSend);
                if(curSend == -1)
                {
                    if(lseek(fd,0,SEEK_CUR)==size)
                        break;
                    else
                        continue;
                }
                numSend += curSend;
            }
        }
        val = 0;
        setsockopt(client_fd,IPPROTO_TCP,TCP_CORK,&val,sizeof(val));
        if(numSend != size)
            LOG_WARN("file: %s has %ld bytes,but send %ld bytes",event->name,size,numSend);
        clock_t end = clock();
        double seconds = (double )(end-start)/CLOCKS_PER_SEC;
        double speed = (double)numSend/(1024 * 1024 * seconds);
        LOG_INFO("end send file: %s,use %.2f seconds,speed %.2f Mb/s",event->name,seconds,speed);
        close(fd);
    }
}
/*
 * when a file was deleted,use this function
 * */
void handleDelete(struct inotify_event *event,int client_fd)
{
    if(event->len > 0)
        sendInfo(client_fd,STR_CMD[DELETE],event->name,NULL);
}
/*
 * when the watched dir was deleted,should notify the pair don't send anything
 * */
void handleDeleteSelf(struct inotify_event *event,int client_fd)
{
    sendInfo(client_fd,STR_CMD[DELETE_SELF],NULL,NULL);
}
/*
 * when the file was renamed,this function was activate
 * */
void handleRename(struct inotify_event *event,int client_fd)
{
    static uint32_t cookie;
    static char old[BUF_SIZE];
    if(event->len > 0)
    {
        if(event->mask & IN_MOVED_TO)
        {
            if(event->cookie == cookie)
            {
                sendInfo(client_fd,STR_CMD[RENAME],old,event->name);
                LOG_INFO("rename file from %s to %s",old,event->name);
            }
        }
        else if(event->mask & IN_MOVED_FROM)
        {
            cookie = event->cookie;
            strcpy(old,event->name);
        }
    }
}

/*
 * handle an inotify event
 * */
void disparityEvent(struct inotify_event *event,int client_fd)
{
    if(CouldSynchronous)
    {
        if(event->mask & IN_CLOSE_WRITE)
            handleCloseWrite(event,client_fd);
        if(event->mask & IN_DELETE)
            handleDelete(event,client_fd);
        if(event->mask & IN_DELETE_SELF)
            handleDeleteSelf(event,client_fd);
        if(event->mask & IN_MOVE)
            handleRename(event,client_fd);
    }
    else
    {
        LOG_WARN("since the pair is not online,file change will not synchronous");
    }
}
/*
 * add a dir to Inotify List
 * */
int addInotifyDirWatch(int inotifyFd,const char *dir,uint32_t mask)
{
    static struct stat stat_buf;
    if(stat(dir,&stat_buf) == -1 || !S_ISDIR(stat_buf.st_mode))
    {
        LOG_ERROR("file: %s is not exists or not a dir",dir);
        return -1;
    }
    return inotify_add_watch(inotifyFd,dir,mask);
}
/*
 * read inotify event from kernel
 * */
void handleInotifyEvent(int inotifyFd,int client_fd)
{
    static struct inotify_event *event;
    static char buf[BUF_LEN];
    ssize_t numRead;
    while ((numRead = read(inotifyFd,buf,BUF_LEN)) > 0)
    {
        for(char *p = buf;p<buf + numRead;)
        {
            event = (struct inotify_event *)p;
            disparityEvent(event,client_fd);
            p += sizeof(struct inotify_event) + event->len;
        }
        memset(buf,0,BUF_LEN);
    }
}
/*
 * when a header is not valid,we should drop the remaining data
 * */
void dropData(int fd)
{
    static char buf[BUF_SIZE];
    ssize_t numRead;
    if(isConnected(fd))
    {
        while (TRUE)
        {
            numRead = read(fd,buf,BUF_SIZE);
            if(numRead == -1)
            {
                if(errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                else
                    err_exit("read");
            }
        }
    }
}
/*
 * we receive an update head,we should create and update the file
 * */
void handleTcpUpdate(int client_fd,const char *filename,const char* size)
{
    if(filename == NULL || size == NULL)
        return;

    int fd = open(filename,O_WRONLY|O_TRUNC|O_CREAT,0664);
    if(fd == -1)
    {
        LOG_WARN("Could not open file: %s,so suspend the update",filename);
        return;
    }
    char *endPtr;
    ssize_t totalSize = strtol(size,&endPtr,10);
    if(totalSize < 0)
    {
        LOG_WARN("the header size is %ld,so suspend the update",totalSize);
        return;
    }
    ssize_t curWrite,numWrite=0;
    ssize_t curRead;
    static char buf[BUF_SIZE];
    clock_t start = clock();
    while (numWrite < totalSize)
    {
        curRead = read(client_fd,buf,BUF_SIZE);
        if(curRead == -1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            else
                err_exit("read");
        }
        if((curWrite = write(fd,buf,curRead)) == -1)
        {
            LOG_WARN("write data to disk failed");
            break;
        }
        else
            numWrite+=curWrite;
    }
    clock_t end = clock();
    close(fd);
    if(numWrite != totalSize)
        LOG_WARN("file: %s has %ld bytes,but received %ld bytes",filename,totalSize,numWrite);
    double seconds = (double )(end-start)/CLOCKS_PER_SEC;
    double speed = (double)numWrite/(1024 * 1024 * seconds);
    LOG_INFO("end handleTcpUpdate file: %s,use %.2f seconds,speed %.2f Mb/s",filename,seconds,speed);
}
/*
 * Delete file
 * */
void handleTcpDelete(const char *filename)
{
    if(filename == NULL)
    {
        LOG_WARN("delete filename is NULL");
        return;
    }
    if(remove(filename) == -1)
        LOG_WARN("remove file %s failed",filename);
}
/*
 * Rename a file
 * */
void handleTcpRename(const char *old,const char *new)
{
    if(old==NULL || new == NULL)
    {
        LOG_WARN("old filename or new filename is empty");
        return;
    }
    if(rename(old,new) == -1)
        LOG_WARN("rename file %s to %s failed",old,new);
}

/*
 * receive Tcp data,and handle the event
 * */
void handleTcpEvent(int client_fd)
{
    char buf[BUF_SIZE];
    ssize_t numRead;
    numRead = readLine(client_fd,buf,BUF_SIZE);
    if(numRead == 0)
    {
        LOG_WARN("readLine return zero");
        return;
    }
    if(numRead == BUF_SIZE - 1) //drop the too long data
    {
        LOG_WARN("header is too long,the data will");
        dropData(client_fd);
        return;
    }
    char *cmd,*arg1,*arg2;
    cmd = strtok(buf,"|");

    if(cmd == NULL)
    {
        LOG_WARN("command is NULL,please check the header format");
        dropData(client_fd);
        return;
    }
    if(strcmp(cmd,STR_CMD[MESSAGE]) != 0)
    {
        arg1 = strtok(NULL,"|");
        arg2 = strtok(NULL,"|\n");
    }
    if(strcmp(cmd,STR_CMD[DELETE_SELF]) == 0)
    {
        LOG_WARN("the pair dir was deleted,turn off synchronous");
        CouldSynchronous = FALSE;
    }
    else if(strcmp(cmd,STR_CMD[UNPAIRED]) == 0)
    {
        LOG_WARN("the pair was quit,turn off synchronous");
        CouldSynchronous = FALSE;
    }
    else if(strcmp(cmd,STR_CMD[PAIRED]) == 0)
    {
        CouldSynchronous = TRUE;
        LOG_WARN("the pair is online,turn on synchronous");
    }
    else if(strcmp(cmd,STR_CMD[MESSAGE]) == 0)
    {
        arg1 = strtok(NULL,"|\n");
        LOG_INFO("Receive Message-> %s", (arg1==NULL)?"EMPTY MESSAGE":arg1);
    }
    else if (strcmp(cmd,STR_CMD[RENAME]) == 0)
        handleTcpRename(arg1,arg2);
    else if (strcmp(cmd,STR_CMD[DELETE]) == 0)
        handleTcpDelete(arg1);
    else if(strcmp(cmd,STR_CMD[UPDATE]) == 0)
        handleTcpUpdate(client_fd,arg1,arg2);
    else
        LOG_WARN("UNKNOWN COMMAND");
    dropData(client_fd);
}
int main(int argc,char *argv[])
{
    if(argc!=6|| strcmp(argv[1],"--help") == 0)
        usageErr("%s ip port username password dirname\n",argv[0]);

    int inotifyFd,client_fd,epoll_fd,dir_fd,port;
    port = (int)strtol(argv[2],NULL,10);

    inotifyFd = inotify_init();
    if(inotifyFd == -1)
        errExit("create inotify failed");
    if(setNonBlock(inotifyFd) == -1)
        errExit("setNonBlock inotifyFd");

    dir_fd = addInotifyDirWatch(inotifyFd,argv[5],IN_ALL_EVENTS);
    if( dir_fd == -1)
        errExit("failed to add %s",argv[5]);

    if(chdir(argv[5]) == -1)
        errExit("chdir");

    epoll_fd = epoll_create(2);
    if(epoll_fd < 0)
        errExit("epoll_create");

    client_fd = createClientSocket(argv[1], port);
    if(client_fd < 0)
        errExit("createClientSocket");
    if(setNonBlock(client_fd) == -1)
        errExit("setNonBlock client_fd");

    if(addEpollInLetEvent(epoll_fd,inotifyFd) == -1)
        errExit("addEpollInLetEvent inotifyFd");
    if(addEpollInLetEvent(epoll_fd,client_fd) == -1)
        errExit("addEpollInLetEvent client_fd");

    int eventRead,i;
    struct epoll_event events[2];
    sendInfo(client_fd,STR_CMD[CONNECTING],argv[3],argv[4]);
    while (TRUE)
    {
        eventRead = epoll_wait(epoll_fd,events,2,-1);
        if(eventRead == -1)
        {
            if(errno == EINTR)
                continue;
            else
                errExit("epoll_wait");
        }
        for (i = 0; i < eventRead; ++i) {
            if(events[i].events & EPOLLIN)
            {
                if(events[i].data.fd == inotifyFd)
                    handleInotifyEvent(inotifyFd,client_fd);
                else if(events[i].data.fd == client_fd)
                {
                    if(inotify_rm_watch(inotifyFd,dir_fd) < 0)
                        errExit("inotify_rm_watch failed");
                    handleTcpEvent(client_fd);
                    dir_fd = addInotifyDirWatch(inotifyFd,argv[5],IN_ALL_EVENTS);
                    if(dir_fd < 0)
                        errExit("addInotifyDirWatch failed,probably the dir was deleted");
                }
            }
            if(events[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP))
            {
                close(events[i].data.fd);
                close(epoll_fd);
                close(client_fd);
                errExit("connection was closed");
            }
        }
    }
}