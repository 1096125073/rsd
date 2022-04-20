//
// Created by xia on 2022/3/2.
//

#ifndef NEW_LINUX_ERROR_HANDLERS_H
#define NEW_LINUX_ERROR_HANDLERS_H

void errMsg(const char *format,...);

#ifdef __GNUC__
#define NORETURN __attribute__ ((__noreturn__))
#else
#define NORETURN
#endif

void errExit(const char* format,...) NORETURN;
void err_exit(const char * format,...) NORETURN;
void errExitEn(int errnum,const char* format,...) NORETURN;
void fatal(const char *format,...);
void usageErr(const char *format,...) NORETURN;
void cmdLineErr(const char *format,...) NORETURN;

#endif //NEW_LINUX_ERROR_HANDLERS_H
