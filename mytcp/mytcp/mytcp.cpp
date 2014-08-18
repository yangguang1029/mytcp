//
//  mytcp.cpp
//  mytcp
//
//  Created by guangy on 8/18/14.
//  Copyright (c) 2014 iMAC-0003. All rights reserved.
//

#include "mytcp.h"
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <strings.h>
#include <netdb.h>

#include <iostream>
#include <pthread.h>
#include <unistd.h>
using namespace std;

MyTCP::MyTCP() {
    memset(mInputStream, 0, sizeof(mInputStream));
    memset(mOutputStream, 0, sizeof(mOutputStream));
}

void MyTCP::closeSocket() {
    close(msocket);
}

bool MyTCP::hasError() {
    if (msocket == -1) {
        return true;
    }
    
    int err = errno;
    // 异步socket时允许返回 EINPROGRESS（正在进行中） EAGAIN（异步IO时）
    if (err != EINPROGRESS && err != EAGAIN) {
        return true;
    }
    return false;
}

static int select_version(int socket_fd, size_t timeout) {
    fd_set rset, wset;
    struct timeval tval;
    FD_ZERO(&rset);
    FD_SET(socket_fd, &rset);
    wset = rset;
    tval.tv_sec = timeout;
    tval.tv_usec = 0;
    int ready_n;
    if ((ready_n = select(socket_fd + 1, &rset, &wset, NULL, &tval)) == 0) {
        //close(socket_fd); /* timeout */
        //errno = ETIMEDOUT;
        return (-1);
    }
    struct sockaddr_in sock_addr;
    int error;
    int len;
    if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &error, (socklen_t*)&len) == 0 &&
        error == 0 &&
        getpeername(socket_fd, (struct sockaddr*)&sock_addr, (socklen_t*)&len) == 0) {
        return 0;
    }
    return -1;
}

bool MyTCP::create(const char* ipserver, const int port, int blockSeconds, bool keepAlive) {
    if (ipserver == nullptr || strlen(ipserver) > 15) {
        return false;
    }
    msocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (msocket == -1) {
        closeSocket();
        return false;
    }
    if (keepAlive) {
        bool ka = true;
        if (setsockopt(msocket, SOL_SOCKET, SO_KEEPALIVE, (char*)&ka, sizeof(ka))) {
            closeSocket();
            return false;
        }
    }
    // 设置为非阻塞方式，read时读不到数据返回-1
    fcntl(msocket, F_SETFL, O_NONBLOCK);
    
    in_addr_t addr = inet_addr(ipserver);
    if (addr == INADDR_NONE) {
        closeSocket();
        return false;
    }
    sockaddr_in	addr_in;
    memset((void *)&addr_in, 0, sizeof(addr_in));
	addr_in.sin_family = AF_INET;
	addr_in.sin_port = htons(port);
	addr_in.sin_addr.s_addr = addr;
    
    int retConnect = connect(msocket, (sockaddr *)&addr_in, sizeof(addr_in));
    if(retConnect == -1) {
        if (hasError()) {
            closeSocket();
            return false;
        } else {
            if (select_version(msocket, blockSeconds) != 0) {
                closeSocket();
                return false;
            }
        }
    }
    mInLen		= 0;
	mInStart	= 0;
	mOutLen	= 0;
    
	struct linger so_linger;
	so_linger.l_onoff = 1;
	so_linger.l_linger = 500;
	setsockopt(msocket, SOL_SOCKET, SO_LINGER, (const char*)&so_linger, sizeof(so_linger));
    
	return true;
}  

void MyTCP::flush(){
    ssize_t fu = send(msocket, mOutputStream, mOutLen, 0);
    ssize_t left = mOutLen - fu;
    if (fu > 0) {
        memcpy(mOutputStream, mOutputStream + fu, left);
    }
    mOutLen -= fu;
}

bool MyTCP::sendMsg(const char *msg, ssize_t len) {
    if (msocket == -1) {
        return false;
    }
    if (len > OUTBUFSIZE) {
        return false;
    }
    
    if (mOutLen + len > OUTBUFSIZE) {
        flush();
    }
    if (mOutLen + len > OUTBUFSIZE) {
        return false;
    }
    memcpy(mOutputStream + mOutLen, msg, len);
    mOutLen += len;
    flush();
    return false;
}

