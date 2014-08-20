//
// TCPClient.h
// framework
//
// Created by weitongming on 2014-1-6
// 基于斗地主的tcp的实现，做了一些改进和接口上的更改，以方便绑定给脚本使用

#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include "cocos2d.h"
#include "extensions/cocos-ext.h"
#include "TCPDelegate.h"

#include "TCPSocket.h"
#include <zlib.h>
#include <sys/types.h>

USING_NS_CC;
USING_NS_CC_EXT;

#define HEART_BEAT_SEND_TIME     6.0     // 发送心跳间隔
#define HEART_BEAT_MAX_TIME      10.0    // 心跳最大间隔
#define TIME_BERWEEN_2_GAME      2.0     // 

#define TCP_CONNECT_FAILED       1     // 连接失败
#define TCP_CONNECT_ERROR        2     // 连接错误(中断)

const int BUFFERSIZE = 1024 * 64; // 网络消息缓冲区

class TCPClient : public CCObject
{
public:
    TCPClient();
    TCPClient(const TCPClient &ano); // 拷贝构造函数, 声明但是不实现，防止错误地使用

    virtual ~TCPClient(); // 析构函数

    TCPClient & operator = (const TCPClient &ano); // 赋值运算符,声明但是不实现

public:
    // 设置回调的接口
    void setDelegate(TCPDelegate* delegate);
    
    TCPDelegate* getDelegate();
    
    // 设置重试连接的超时时间
    void setRetryConnectTimeout(int seconds);

    // 设置参数
    void setParams(const char* server, const int port);
    
    // 建立连接
    void connect(double dTimeOut = 0);

    // 关闭链接
    void closeConnect();
    
    // 发送消息
    void sendMsg(const char* msg);

    // 是否已经连接
    bool isConnected();

    // 主循环中的网络tick
    void tickMsg(float dt);

    // 杀掉连接，仅在heart time out的时候调用，模拟一次网络错误
    static void killConnect();
    
    // 销毁，销毁之后该对象就不能再用了
    void destroy();
private: // 成员变量
    char    m_cNetBuffer[BUFFERSIZE];
    
    int m_nRetryConnectTimeout;    // 重连超时时间，即当断网后进行重连，如果在这段时间内还是没有连上就不再重连了
    int m_nSeed;
    
    TCPDelegate* m_pDelegate;
    pthread_t pid;              // 处理connect阻塞的线程id

    std::string m_strServer;
    int m_nPort;
    
    bool        m_bWaitingCreateSocket;         // 等待创建socket
    bool        m_bNeedConnectAgain;            // 需要再次连接（连接时正在等待创建socket）
    double      m_dConnectTimeOut;              // 连接超时
};

#endif
