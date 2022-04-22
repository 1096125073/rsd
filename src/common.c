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

Boolean sendInfo(int fd,const char *header_type,const char *arg1,const char *arg2)
{
    if(header_type == NULL)
        return FALSE;
    static char buf[BUF_SIZE];
    if(arg1 == NULL)
        arg1 = "";
    if(arg2 == NULL)
        arg2 = "";
    sprintf(buf,"%s|%s|%s\r\n",header_type,arg1,arg2);
    return write(fd,buf, strlen(buf)) == strlen(buf);
}

Boolean sendMsg(int fd,const char *format,...)
{
    va_list ap;
    va_start(ap,format);
    static char buf[BUF_SIZE];
    vsnprintf(buf,BUF_SIZE,format,ap);
    va_end(ap);
    return sendInfo(fd,STR_CMD[MESSAGE],buf,NULL);
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
int addEpollInLevelEvent(int epoll_fd,int fd)
{
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLRDHUP;
    event.data.fd = fd;
    return epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&event);
}
