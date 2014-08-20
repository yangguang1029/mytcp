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

void MyTCP::destroy() {
    if( msocket == -1){
        return;
    }
    
	// 关闭
	struct linger so_linger;
	so_linger.l_onoff = 1;
	so_linger.l_linger = 500;
	int ret = setsockopt(msocket, SOL_SOCKET, SO_LINGER, (const char*)&so_linger, sizeof(so_linger));
    
    closeSocket();
    
	msocket = -1;
	mInLen = 0;
	mInStart = 0;
	mOutLen = 0;
    
	memset(mInputStream, 0, sizeof(INBUFSIZE));
	memset(mOutputStream, 0, sizeof(OUTBUFSIZE));
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
    return true;
}

bool MyTCP::recvFromSocket() {
    if (mInLen > INBUFSIZE || msocket == -1) {
        return false;
    }
    ssize_t use = 0;
    if (mInStart + mInLen >= INBUFSIZE) {       //已经绕过环尾，可用空间就是总大小-已用大小
        use = INBUFSIZE - mInLen;
    } else {    //先填满到环尾，再从环头填充
        use = INBUFSIZE - (mInStart + mInLen);
    }
    int savepos = (mInStart + mInLen) % INBUFSIZE;
    ssize_t inlen = recv(msocket, mInputStream + savepos, use, 0);
    if (inlen > 0) {
        mInLen += inlen;
        if (mInLen > INBUFSIZE) {
            return false;
        }
        if (inlen == use && mInLen < INBUFSIZE) {   //后一个条件必须，也屏蔽了前面已绕过环尾的情况
            int left = INBUFSIZE - mInLen;
            ssize_t inlen1 = recv(msocket, mInputStream, left, 0);
            if (inlen1 > 0) {
                mInLen += inlen1;
                if (mInLen > INBUFSIZE) {
                    return false;
                }
            } else if(inlen1 == 0) {
                destroy();
                return false;
            } else {
                if (hasError()) {
                    destroy();
                    return false;
                }
            }
        }
    } else if(inlen == 0) { //??
        destroy();
        return false;
    } else {
        if (hasError()) {
            destroy();
            return false;
        }
        //??
    }
    return true;
}

bool MyTCP::receiveMsg(char *msg, ssize_t &len) {
    if (msocket == -1 || msg == nullptr) {
        return false;
    }
    if (mInLen < 4) {
        bool success = recvFromSocket();
        if (!success || mInLen < 4) {
            return false;
        }
    }
    int packsize = 0;
    if (mInLen == 4) {  //如果inlen == 4，则不管开头存的数据是多少，大小只是4
        packsize = 4;
    } else {    //考虑头部环绕的可能
        if (mInStart + 4 > INBUFSIZE) {
            int aLen = INBUFSIZE - mInStart;
            char* cLen = new char[4];
            memset(cLen, 0, 4);
            memcpy(cLen, mInputStream + mInStart, aLen);
            memcpy(cLen + aLen, mInputStream, 4 - aLen);
            sscanf(cLen, "%4d", &packsize);
            delete [] cLen;
            cLen = NULL;
        } else {
            sscanf(mInputStream + mInStart, "%4d", &packsize);
        }
        packsize += 4;
    }
    if (packsize > _MAX_MSGSIZE) {  //等于抛弃已有数据
        mInStart = 0;
        mInLen = 0;
        return false;
    }
    if (packsize > mInLen) {    //有数据还没到
        bool success = recvFromSocket();
        if (!success || packsize > mInLen) {    //再次请求失败或者仍然数据不够
            return false;
        }
    }
    //拷贝数据
    if (mInStart + mInLen > INBUFSIZE) {    //有回环
        int first_len = INBUFSIZE - mInStart;
        memcpy(msg, mInputStream + mInStart, first_len);
        memcpy(msg + first_len, mInputStream, packsize - first_len);
        len = packsize;
    } else {    //不用inlen，而是用packsize，理论上二者应该相等，但packsize更可靠些
        len = packsize;
        memcpy(msg, mInputStream + mInStart, packsize);
    }
    mInStart = (mInStart + mInLen) % INBUFSIZE;
    mInLen = 0;
    return true;
}

