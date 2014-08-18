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

class MyTCP {
public:
    bool create(const char* ipserver, const int port, int blockSeconds = 30, bool keepAlive = false);
    MyTCP();
    
    bool sendMsg(const char* msg, ssize_t len);
private:
    void closeSocket();
    bool hasError();
    void flush();
private:
    int msocket;
    char mInputStream[INBUFSIZE];
    char mOutputStream[OUTBUFSIZE];
    
    int mInLen;
    int mOutLen;
    int mInStart;
};


#endif /* defined(__mytcp__mytcp__) */
