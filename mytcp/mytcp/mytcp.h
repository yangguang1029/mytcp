//
//  mytcp.h
//  mytcp
//
//  Created by guangy on 8/18/14.
//  Copyright (c) 2014 iMAC-0003. All rights reserved.
//

#ifndef __mytcp__mytcp__
#define __mytcp__mytcp__

#include <iostream>

#define INBUFSIZE	(64*1024*2)
#define OUTBUFSIZE	(64*1024)
#define _MAX_MSGSIZE (64 * 1024 - 4)    //单个消息最大

class MyTCP {
public:
    bool create(const char* ipserver, const int port, int blockSeconds = 30, bool keepAlive = false);
    MyTCP();
    
    bool sendMsg(const char* msg, ssize_t len);
    bool receiveMsg(char* msg, ssize_t& len);
private:
    void closeSocket();
    bool hasError();
    void flush();
    bool recvFromSocket();
    void destroy();
private:
    int msocket;
    char mInputStream[INBUFSIZE];
    char mOutputStream[OUTBUFSIZE];
    
    int mInLen;     //表示已经收到的数据长度，消息可能分几个批次到达，mInLen表示累计收到的长度之和
    int mOutLen;    //表示需要发送的数据，剩余长度
    int mInStart;   //收消息环形缓冲区，表示消息起点
};


#endif /* defined(__mytcp__mytcp__) */
