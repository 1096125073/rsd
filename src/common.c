//
// Created by xia on 2022/4/15.
//

#include "common.h"

ssize_t readLine(int sock,char *buf,int size)
{
    int i = 0;
    char c = '\0';
    ssize_t n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';
    return i;
}

void sendMsg(int fd,int code,const char *format,...)
{
    static char buf[BUF_SIZE];
    sprintf(buf,"%s %d ",STR_CMD[MESSAGE],code);
    write(fd,buf, strlen(buf));
    va_list ap;
    va_start(ap,format);
    vsnprintf(buf,BUF_SIZE,format,ap);
    write(fd,buf, strlen(buf));
    va_end(ap);
    write(fd,"\r\n",2);
}

Boolean isSock(int fd)
{
    static struct stat stat_buf;
    if(fstat(fd,&stat_buf) == 0)
    {
        if(S_ISSOCK(stat_buf.st_mode))
            return TRUE;
    }
    return FALSE;
}

void safeCloseSock(int fd)
{
    if(isSock(fd))
        close(fd);
}

int isConnected(int fd)
{
    if(!isSock(fd))
        return 0;
    static struct tcp_info info;
    socklen_t size = sizeof(info);
    getsockopt(fd,IPPROTO_TCP,TCP_INFO,&info,&size);
    if(info.tcpi_state == TCP_ESTABLISHED)
        return 1;
    return 0;
}
int setNonBlock(int fd)
{
    int flag = fcntl(fd,F_GETFL);
    return fcntl(fd,F_SETFL,flag|O_NONBLOCK);
}
int addEpollInLetEvent(int epoll_fd,int fd)
{
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    event.data.fd = fd;
    return epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&event);
}
Boolean sendHeader(int client_fd,const char*cmd,const char *arg1,const char *arg2)
{
    if(cmd == NULL)
        return FALSE;
    if(arg1 == NULL)
        arg1 = "";
    if(arg2 == NULL)
        arg2 = "";
    static char buf[BUF_SIZE];
    snprintf(buf,BUF_SIZE,"%s %s %s\n",cmd,arg1,arg2);
    return write(client_fd,buf, strlen(buf)) == strlen(buf);
}