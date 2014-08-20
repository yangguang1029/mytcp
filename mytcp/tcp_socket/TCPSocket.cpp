//
//  CNativeSocket.cpp
//  BetCastle
//
//  Created by mac on 13-12-11.
//
//

#include "TCPSocket.h"

TCPSocket::TCPSocket()
{ 
	// 初始化
	memset(m_bufOutput, 0, sizeof(m_bufOutput));
	memset(m_bufInput, 0, sizeof(m_bufInput));
}

void TCPSocket::closeSocket()
{
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
    closesocket(m_sockClient);
    WSACleanup();
#elif (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
    shutdown(m_sockClient, SHUT_RDWR);
#else
    close(m_sockClient);
#endif
//    m_sockClient = INVALID_SOCKET;
}

static int select_version(int socket_fd, size_t timeout) {
    fd_set rset, wset;
    struct timeval tval;
    FD_ZERO(&rset);
    FD_SET(socket_fd, &rset);
    wset = rset;
    tval.tv_sec = timeout;
    tval.tv_usec = 0; //300毫秒
    int ready_n;
    CCLOG("[TCP]socket_fd = %d\n", socket_fd);
    if ((ready_n = select(socket_fd + 1, &rset, &wset, NULL, &tval)) == 0) {
        //close(socket_fd); /* timeout */
        //errno = ETIMEDOUT;
        CCLOG("[TCP]select timeout: %s\n", strerror(errno));
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
    CCLOG("[TCP]select error: %d, %s\n", errno, strerror(errno));
    return -1;
}

bool TCPSocket::Create(const char* pszServerIP, int nServerPort, int nBlockSec, bool bKeepAlive /*= FALSE*/)
{
	// 检查参数
	if(pszServerIP == NULL || strlen(pszServerIP) > 15) {
		return false;
	}

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
	WSADATA wsaData;
	WORD version = MAKEWORD(2, 0);
	int ret = WSAStartup(version, &wsaData);//win sock start up
	if (ret != 0) {
		return false;
	}
#endif

	// 创建主套接字
	m_sockClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(m_sockClient == INVALID_SOCKET) {
        closeSocket();
		return false;
	}

	// 设置SOCKET为KEEPALIVE, 默认的心跳时间大概是2个小时，所以还是需要有客户端自己的心跳机制
	if(bKeepAlive)
	{
		int		optval=1;
		if(setsockopt(m_sockClient, SOL_SOCKET, SO_KEEPALIVE, (char *) &optval, sizeof(optval)))
		{
            closeSocket();
			return false;
		}
	}

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
	DWORD nMode = 1;
	int nRes = ioctlsocket(m_sockClient, FIONBIO, &nMode);
	if (nRes == SOCKET_ERROR) {
		closeSocket();
		return false;
	}
#else
	// 设置为非阻塞方式，read时读不到数据返回-1
	fcntl(m_sockClient, F_SETFL, O_NONBLOCK);
#endif

	unsigned long serveraddr = inet_addr(pszServerIP);
	if(serveraddr == INADDR_NONE)	// 检查IP地址格式错误
	{
		closeSocket();
		return false;
	}

	sockaddr_in	addr_in;
	memset((void *)&addr_in, 0, sizeof(addr_in));
	addr_in.sin_family = AF_INET;
	addr_in.sin_port = htons(nServerPort);
	addr_in.sin_addr.s_addr = serveraddr;
	
    CCLOG("[TCP]connect ip = %s, port = %d", pszServerIP, nServerPort);
    int retConnect = connect(m_sockClient, (sockaddr *)&addr_in, sizeof(addr_in));
    CCLOG("[TCP]connect result = %d", retConnect);
	if(retConnect == SOCKET_ERROR) {
		if (hasError()) {
            Destroy();
			return false;
		}
		else	// WSAWOLDBLOCK
		{
            CCLOG("[TCP]select_version");
            if (select_version(m_sockClient, nBlockSec) != 0) {
                close(m_sockClient);
                return false;
            }
            /*
            // 处理因为异步导致的connect的问题
			timeval timeout;
			timeout.tv_sec	= nBlockSec;
			timeout.tv_usec	= 0;
			fd_set writeset, exceptset, readset;
            FD_ZERO(&readset);
			FD_ZERO(&writeset);
			FD_ZERO(&exceptset);
            FD_SET(m_sockClient, &readset);
			FD_SET(m_sockClient, &writeset);
			FD_SET(m_sockClient, &exceptset);

			int sret = select(FD_SETSIZE, &readset, &writeset, &exceptset, &timeout);
            CCLOG("[TCP]select result = %d", sret);
			if (INVALID_SOCKET == m_sockClient || sret == 0 || sret < 0) {
                CCLOG("[TCP]INVALID_SOCKET == m_sockClient || ret == 0 || ret < 0");
				closeSocket();
				return false;
			} else	// ret > 0
			{
                CCLOG("[TCP]ret > 0");
                
                // 有可写的时候判断是否是出现异常
				int fret1 = FD_ISSET(m_sockClient, &exceptset);
                int fret2 = FD_ISSET(m_sockClient, &writeset);
                int fret3 = FD_ISSET(m_sockClient, &readset);
                CCLOG("[TCP] socket = %d, in writeset = %d, in exceptset = %d, in readset = %d", m_sockClient, fret2, fret1, fret3);
                
				if(fret2 && fret3)		// or (!FD_ISSET(m_sockClient, &writeset)
				{
					closeSocket();
					return false;
				}
			}*/
		}
	}

	m_nInbufLen		= 0;
	m_nInbufStart	= 0;
	m_nOutbufLen	= 0;

	struct linger so_linger;
	so_linger.l_onoff = 1;
	so_linger.l_linger = 500;
	setsockopt(m_sockClient, SOL_SOCKET, SO_LINGER, (const char*)&so_linger, sizeof(so_linger));

	return true;
}

bool TCPSocket::SendMsg(void* pBuf, int nSize)
{
	if(pBuf == 0 || nSize <= 0) {
		return false;
	}
    if (nSize >= OUTBUFSIZE) {
//        LOGE("too long msg....");
        return false;
    }

	if (m_sockClient == INVALID_SOCKET) {
		return false;
	}

	// 检查通讯消息包长度
	int packsize = 0;
	packsize = nSize;

	// 检测BUF溢出
	if(m_nOutbufLen + nSize > OUTBUFSIZE) {
		// 立即发送OUTBUF中的数据，以清空OUTBUF。
		Flush();
		if(m_nOutbufLen + nSize > OUTBUFSIZE) {
			// 出错了
			Destroy();
			return false;
		}
	}
	// 数据添加到BUF尾
	memcpy(m_bufOutput + m_nOutbufLen, pBuf, nSize);
	m_nOutbufLen += nSize;
	return true;
}

bool TCPSocket::ReceiveMsg(void* pBuf, int& nSize)
{
	//检查参数
	if(pBuf == NULL || nSize <= 0) {
		return false;
	}
	
	if (m_sockClient == INVALID_SOCKET) {
		return false;
	}

	// 检查是否有一个消息(小于4则无法获取到消息长度)
	if(m_nInbufLen < 4) {
		//  如果没有请求成功  或者   如果没有数据则直接返回
		if(!recvFromSock() || m_nInbufLen < 4) {		// 这个m_nInbufLen更新了
			return false;
		}
	}
    
    int packsize = 0;
    
    // 先发种子，不会发送别的东西，所以可以用strlen(m_bufInput) == 4来确定
    if( strlen(m_bufInput) == 4 ){
        // 种子，返回种子
        packsize = 4;
    } else {
        // 计算要拷贝的消息的大小，前四个字节为消息长度，要考虑横跨环形缓冲区的情况
        if (m_nInbufStart + 4 > INBUFSIZE ) {
            int aLen = INBUFSIZE - m_nInbufStart;
            char* cLen = new char[4];
            memset(cLen, 0, 4);
            memcpy(cLen, m_bufInput + m_nInbufStart, aLen);
            memcpy(cLen + aLen, m_bufInput, 4 - aLen);
            sscanf(cLen, "%4x", &packsize);
            delete [] cLen;
            cLen = NULL;
        } else {
            sscanf(m_bufInput + m_nInbufStart, "%4x", &packsize);
        }
//        LOGD("msg length is %d", packsize);
        
        // packsize为后续包的长度，总的包的长度
        packsize += 4;
//        LOGD("total msg length is %d", packsize);
    }
    
	// 检测消息包尺寸错误 暂定最大64k
	if (packsize <= 0 || packsize > _MAX_MSGSIZE) {
		m_nInbufLen = 0;		// 直接清空INBUF
		m_nInbufStart = 0;
		return false;
	}

	// 检查消息是否完整(如果将要拷贝的消息大于此时缓冲区数据长度，需要再次请求接收剩余数据)
	if (packsize > m_nInbufLen) {
		// 如果没有请求成功   或者    依然无法获取到完整的数据包  则返回，直到取得完整包
		if (!recvFromSock() || packsize > m_nInbufLen) {	// 这个m_nInbufLen已更新
			return false;
		}
	}

	// 复制出一个消息
	if(m_nInbufStart + packsize > INBUFSIZE) {
		// 如果一个消息有回卷（被拆成两份在环形缓冲区的头尾）
		// 先拷贝环形缓冲区末尾的数据
		int copylen = INBUFSIZE - m_nInbufStart;
		memcpy(pBuf, m_bufInput + m_nInbufStart, copylen);

		// 再拷贝环形缓冲区头部的剩余部分
		memcpy((unsigned char *)pBuf + copylen, m_bufInput, packsize - copylen);
		nSize = packsize;
	} else {
		// 消息没有回卷，可以一次拷贝出去
		memcpy(pBuf, m_bufInput + m_nInbufStart, packsize);
		nSize = packsize;
	}

	// 重新计算环形缓冲区头部位置
	m_nInbufStart = (m_nInbufStart + packsize) % INBUFSIZE;
	m_nInbufLen -= packsize;
	return	true;
}

bool TCPSocket::hasError()
{
    if( m_sockClient == INVALID_SOCKET ){
        return true;
    }
    
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
	int err = WSAGetLastError();
	if(err != WSAEWOULDBLOCK) {
#else
	int err = errno;
    
    // 异步socket时允许返回 EINPROGRESS（正在进行中） EAGAIN（异步IO时）
	if(err != EINPROGRESS && err != EAGAIN) {
#endif
		return true;
	}

	return false;
}

// 从网络中读取尽可能多的数据，实际向服务器请求数据的地方
bool TCPSocket::recvFromSock(void)
{
	if (m_nInbufLen >= INBUFSIZE || m_sockClient == INVALID_SOCKET) {
		return false;
	}

	// 接收第一段数据
	int	savelen, savepos;			// 数据要保存的长度和位置
	if(m_nInbufStart + m_nInbufLen < INBUFSIZE)	{	// INBUF中的剩余空间有回绕
		savelen = INBUFSIZE - (m_nInbufStart + m_nInbufLen);		// 后部空间长度，最大接收数据的长度
	} else {
		savelen = INBUFSIZE - m_nInbufLen;
	}

	// 缓冲区数据的末尾
	savepos = (m_nInbufStart + m_nInbufLen) % INBUFSIZE;
	CHECKF(savepos + savelen <= INBUFSIZE);
	int inlen = recv(m_sockClient, m_bufInput + savepos, savelen, 0);
	if(inlen > 0) {
		// 有接收到数据
		m_nInbufLen += inlen;
		
		if (m_nInbufLen > INBUFSIZE) {
			return false;
		}

		// 接收第二段数据(一次接收没有完成，接收第二段数据)
		if(inlen == savelen && m_nInbufLen < INBUFSIZE) {
			int savelen = INBUFSIZE - m_nInbufLen;
			int savepos = (m_nInbufStart + m_nInbufLen) % INBUFSIZE;
			CHECKF(savepos + savelen <= INBUFSIZE);
			inlen = recv(m_sockClient, m_bufInput + savepos, savelen, 0);
			if(inlen > 0) {
				m_nInbufLen += inlen;
				if (m_nInbufLen > INBUFSIZE) {
					return false;
				}	
			} else if(inlen == 0) {
				Destroy();
				return false;
			} else {
				// 连接已断开或者错误（包括阻塞）
				if (hasError()) {
					Destroy();
					return false;
				}
			}
		}
	} else if(inlen == 0) {
		Destroy();
		return false;
	} else {
		// 连接已断开或者错误（包括阻塞）
		if (hasError()) {
			Destroy();
			return false;
		}
	}

	return true;
}

bool TCPSocket::Flush(void)		//? 如果 OUTBUF > SENDBUF 则需要多次SEND（）
{
	if (m_sockClient == INVALID_SOCKET) {
		return false;
	}

	if(m_nOutbufLen <= 0) {
		return true;
	}
	
	// 发送一段数据
	int	outsize;
	outsize = send(m_sockClient, m_bufOutput, m_nOutbufLen, 0);
	if(outsize > 0) {
		// 删除已发送的部分
		if(m_nOutbufLen - outsize > 0) {
			memcpy(m_bufOutput, m_bufOutput + outsize, m_nOutbufLen - outsize);
		}

		m_nOutbufLen -= outsize;

		if (m_nOutbufLen < 0) {
			return false;
		}
	} else {
		if (hasError()) {
			Destroy();
			return false;
		}
	}

	return true;
}

bool TCPSocket::Check(void)
{
	// 检查状态
	if (m_sockClient == INVALID_SOCKET) {
		return false;
	}

	char buf[1];
	int	ret = recv(m_sockClient, buf, 1, MSG_PEEK);
	if(ret == 0) {
		Destroy();
		return false;
	} else if(ret < 0) {
		if (hasError()) {
			Destroy();
			return false;
		} else {	// 阻塞
			return true;
		}
	} else {	// 有数据
		return true;
	}
	
	return true;
}

void TCPSocket::Destroy(void)
{
    if( m_sockClient == INVALID_SOCKET){
        return;
    }
    
	// 关闭
	struct linger so_linger;
	so_linger.l_onoff = 1;
	so_linger.l_linger = 500;
	int ret = setsockopt(m_sockClient, SOL_SOCKET, SO_LINGER, (const char*)&so_linger, sizeof(so_linger));

    closeSocket();

	m_sockClient = INVALID_SOCKET;
	m_nInbufLen = 0;
	m_nInbufStart = 0;
	m_nOutbufLen = 0;

	memset(m_bufOutput, 0, sizeof(m_bufOutput));
	memset(m_bufInput, 0, sizeof(m_bufInput));
}

