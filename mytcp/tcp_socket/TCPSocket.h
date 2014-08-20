//
//  TCPSocket.h
//  framework
//
//  Created by mac on 13-12-11.
//
//  modified by weitongming on 2014-01-06, 为便于进行脚本的回调进行了修改

#include "cocos2d.h"

// CC_PLATFORM_IOS CC_PLATFORM_WIN32 CC_PLATFORM_ANDROID

// 平台宏 改为使用cocos2dx当中的
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
#include <windows.h>
#include <WinSock.h>
#else
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

#define SOCKET int
#define SOCKET_ERROR    -1
#define INVALID_SOCKET  -1
#endif

#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
#include <sys/_select.h>
#endif

#ifndef CHECKF
#define CHECKF(x) \
do \
{ \
if (!(x)) { \
CCLOG("CHECKF", #x, __FILE__, __LINE__); \
return 0; \
} \
} while (0)
#endif

#define _MAX_MSGSIZE (64 * 1024 - 4)    // 暂定一个消息最大为64k，语音消息最大64K
#define BLOCKSECONDS	20              // INIT函数阻塞时间，单位s
#define INBUFSIZE	(64*1024*2)         // 具体尺寸根据剖面报告调整，接收数据的缓存
#define OUTBUFSIZE	(64*1024)           // 具体尺寸根据剖面报告调整，发送数据的缓存，语音消息最大64K。当不超过64K时，FLUSH只需要SEND一次

class TCPSocket {
public:
	TCPSocket();
	bool	Create(const char* pszServerIP, int nServerPort, int nBlockSec = BLOCKSECONDS, bool bKeepAlive = false);
	bool	SendMsg(void* pBuf, int nSize);
	bool	ReceiveMsg(void* pBuf, int& nSize);
	bool	Flush(void);
	bool	Check(void);
	void	Destroy(void);
	SOCKET	GetSocket(void) const { return m_sockClient; }
    
private:
	bool	recvFromSock(void);		// 从网络中读取尽可能多的数据
	bool    hasError();			    // 是否发生错误，注意，异步模式未完成非错误
	void    closeSocket();
    
	SOCKET	m_sockClient;
    
	// 发送数据缓冲
	char	m_bufOutput[OUTBUFSIZE];	//? 可优化为指针数组
	int		m_nOutbufLen;
    
	// 环形缓冲区
	char	m_bufInput[INBUFSIZE];
	int		m_nInbufLen;
	int		m_nInbufStart;				// INBUF使用循环式队列，该变量为队列起点，0 - (SIZE-1)
};

