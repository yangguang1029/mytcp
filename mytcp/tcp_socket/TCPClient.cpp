//
// TCPClient.cpp
// framework
//
// Created by weitongming on 2014-1-6
// 基于斗地主的tcp的实现，做了一些改进和接口上的更改，以方便绑定给脚本使用
#include "CConnectUtil.h"
#include "TCPClient.h"

TCPSocket gSocket;                      // 全局socket对象
bool gSocketCreateSuccess = false;      // socket是否创建成功
bool gIsCreatingSocket = false;         // 是否正在创建socket（其他线程只读，为避免异步问题，改标志为true时应避免对socket做任何操作）

// 创建socket参数结构
struct CreateSocketParam
{
    std::string         strServer;      // 服务器地址
    int                 nPort;          // 端口
    double              dTimeOut;       // 超时时间（秒）
};

// 创建socket
void *_CREATE_SOCKET(void* pParam)
{
    assert(pParam);
    struct CreateSocketParam* pCreateSocketParam = (struct CreateSocketParam*)pParam;
    
    // 保存参数
    const std::string strServer = pCreateSocketParam->strServer;
    const int nPort = pCreateSocketParam->nPort;
    double dTimeOut = pCreateSocketParam->dTimeOut;
    
    // 删除参数
    delete pCreateSocketParam;
    pCreateSocketParam = NULL;
    
    // 记录开始时间
    timeval start, now;
    if(gettimeofday(&start, NULL) != 0)
    {
        assert(0);
        return 0;
    }
    
    // 设置标志
    gIsCreatingSocket = true;
    gSocketCreateSuccess = false;
    bool bCreatSuccess = false;
    
    // 先销毁一下
    gSocket.Destroy();
    
    // 创建
    while(true)
    {
        if(!bCreatSuccess)
        {
            CCLOG("[TCP]Socket.Create");
            bCreatSuccess = gSocket.Create(strServer.c_str(), nPort, 10);
            if(bCreatSuccess)
            {
                CCLOG("[TCP]Socket.Create success.");                
            }
        }
        
        // 创建成功
        if(bCreatSuccess)
        {
            CCLOG("[TCP]Socket.Check");
            if(gSocket.Check())
            {
                CCLOG("[TCP]Socket.Check success.");
                gSocketCreateSuccess = true;
                break;
            }
            else
            {
                CCLOG("[TCP]Socket.Check failed.");
            }
        }
        
        if(gettimeofday(&now, NULL) != 0)
        {
            assert(0);
            break;
        }
        
        // 超时退出
        const double dTimePassed = (now.tv_sec - start.tv_sec) + (now.tv_usec - start.tv_usec) / 1000000.0f;
        CCLOG("[TCP]Socket create time passed = %f", dTimePassed);
        if(dTimePassed > dTimeOut)
        {
            break;
        }
        
        usleep(1000000);
    }

    // 创建成功，但是没有Check成功
    if(bCreatSuccess && !gSocketCreateSuccess)
    {
        gSocket.Destroy();
    }

    gIsCreatingSocket = false;
    return 0;
}

TCPClient::TCPClient()
: m_nPort(-1)
, m_nRetryConnectTimeout(35)
, m_nSeed(0)
, m_pDelegate(NULL)
, m_strServer("")
, m_bWaitingCreateSocket(false)
, m_bNeedConnectAgain(false)
, m_dConnectTimeOut(0)
{
    // 初始化代码
    Scheduler *sch = Director::getInstance()->getScheduler();
    sch->schedule(schedule_selector(TCPClient::tickMsg), this, 0.0, false);
}

TCPClient::~TCPClient()
{
    this->destroy();
}

void TCPClient::killConnect()
{
    if(gIsCreatingSocket)
    {
        CCLOG("[TCP]Can not kill connect while creating socket.");
        return;
    }
    
    // 干掉socket的资源
    gSocket.Destroy();
    
    CCLOG("[TCP]Connection has been killed.");
}

void TCPClient::destroy()
{
    // 退出线程
    if(gIsCreatingSocket){
        pthread_kill(pid, 1000);
    }
    
    // 关闭连接
    if(gSocketCreateSuccess)
    {
        this->closeConnect();
    }
    
    if (NULL != m_pDelegate){
        delete m_pDelegate;
        m_pDelegate = NULL;
    }
    
    // 停止更新
    Scheduler *sch = Director::getInstance()->getScheduler();
    sch->unscheduleAllForTarget(this);
}

void TCPClient::closeConnect()
{
    if(gIsCreatingSocket)
    {
        CCLOG("[TCP]Can not close connect while creating socket.");
        return;
    }
    
    // 还没有连接
    if(!gSocketCreateSuccess)
    {
        CCLOG("[TCP]Can not close connect while socket has not been created.");
        return;
    }
    
    // 干掉socket的资源
    gSocket.Destroy();
    gSocketCreateSuccess = false;
    
    // 回调
    m_pDelegate->onClosed();
    
    CCLOG("[TCP]Connection has been closed.");
}

void TCPClient::setParams(const char* server, const int port)
{
    // 函数体
    m_strServer = server;
    m_nPort     = port;
}

void TCPClient::connect(double dTimeOut/* = 0*/)
{
    m_dConnectTimeOut = dTimeOut;
    if(gIsCreatingSocket)
    {
        m_bNeedConnectAgain = true;
        CCLOG("[TCP]Neet connect again while creating socket.");
        return;
    }
    
    if( isConnected() ){
        closeConnect();
    }
    
    assert(0 != m_nPort);
    assert(!m_strServer.empty());
    
    m_nSeed = 0;
    
    // 转到另一个线程中进行阻塞的操作
    struct CreateSocketParam* pParam = new CreateSocketParam;
    pParam->strServer = m_strServer;
    pParam->nPort = m_nPort;
    pParam->dTimeOut = dTimeOut;
    if(0 != pthread_create(&pid, NULL, _CREATE_SOCKET, (void *)pParam))
    {
        delete pParam;
        pParam = NULL;
        
        CCLOG("[TCP]Create create socket thread failed.");
        return;
    }
    
    // 等待创建线程回应
    m_bWaitingCreateSocket = true;
    
    CCLOG("[TCP]Connecting time out = %f", dTimeOut);
}

bool TCPClient::isConnected()
{
    if(gIsCreatingSocket)
        return false;

    if(!gSocketCreateSuccess)
        return false;
    
    return gSocket.Check();
}

void TCPClient::sendMsg(const char* msg)
{
    // 正在连接，返回
    if(gIsCreatingSocket)
        return;
    
    // 没有连接成功，返回
    if(!gSocketCreateSuccess)
        return;
    
    // 函数体
    if( BUFFERSIZE < strlen(msg) + 4 ){
        CCLOG("LONGNET Too large msg.....");
        return;
    }
    
    memset(m_cNetBuffer, 0, BUFFERSIZE);
    unsigned long nlen = strlen(msg);
    // LOGD("LONGNET data length is %d", nlen);
    
    char dLen[5] = {0};
    sprintf(dLen, "%04lx", nlen);
    // LOGD("LONGNET data length is %s", dLen);
    
    memccpy(m_cNetBuffer, dLen, '\0', 4);
    memccpy(m_cNetBuffer + 4, msg, '\0', nlen);
    
    // LOGD("LONGNET total length is %d", 4 + nlen);
    
    // 做异或的处理
    CConnUtil::xorCodeing(m_nSeed + nlen, (unsigned char*)(m_cNetBuffer + 4), nlen);
    // LOGD("LONGNET Send data length is %d", strlen(m_cNetBuffer));
    if(!gSocket.SendMsg((void *)m_cNetBuffer, 4 + nlen))
    {
        CCLOG("[TCP]Send msg failed. msg[%s]", msg);
    }
    //CCLOG("[TCP]Send msg[%s]", msg);
}

void TCPClient::setDelegate(TCPDelegate* delegate)
{
    // 函数体
    m_pDelegate = delegate;
}

TCPDelegate* TCPClient::getDelegate()
{
    return m_pDelegate;
}

void TCPClient::setRetryConnectTimeout(int seconds)
{
    // 函数体
     m_nRetryConnectTimeout = seconds;
}

void TCPClient::tickMsg(float dt)
{
    // 正在连接，返回
    if(gIsCreatingSocket)
        return;
 
    // 创建Socket结果
    if(m_bWaitingCreateSocket)
    {
        // 成功
        if(gSocketCreateSuccess)
        {
            CCLOG("[TCP]Create socket success.");
            
            // 需要再次连接
            if(m_bNeedConnectAgain)
            {
                CCLOG("[TCP]Connect Again.");
                
                gSocketCreateSuccess = false;
                this->connect(m_dConnectTimeOut);
            }
        }
        // 失败
        else
        {
            CCLOG("[TCP]Create socket failed.");
            if(m_pDelegate) m_pDelegate->onError(TCP_CONNECT_FAILED);
        }
        m_bWaitingCreateSocket = false;
        m_bNeedConnectAgain = false;
    }
    
    // 没有连接成功，返回
    if(!gSocketCreateSuccess)
        return;

    // 检查连接状态
    if (!gSocket.Check())
    {
        // 掉线了
        CCLOG("[TCP]Socket error.");
        gSocketCreateSuccess = false;
        
        // 错误回调
        if(m_pDelegate) m_pDelegate->onError(TCP_CONNECT_ERROR);
        
        // 重连
        this->connect(m_nRetryConnectTimeout);
        
        // 重连回调
        if(m_pDelegate) m_pDelegate->onReconnect();
        
        return;
    }
    
    // 1、发送数据（向服务器发送消息）
    gSocket.Flush();
    
    // 2、接收数据（取得缓冲区中的所有消息，直到缓冲区为空）
    while (true)
    {
        char buffer[_MAX_MSGSIZE] = { 0 };
        int nSize = sizeof(buffer);
        char* pbufMsg = buffer;

        // 循环，直到取不到数据
        if (!gSocket.ReceiveMsg(pbufMsg, nSize)) {
            break;
        } else {
            // LOGD("LONGNET Receive msg %s and msg size is %d", pbufMsg, nSize);
        }
        
        // 处理消息。。。。
        if( 0 == m_nSeed && 4 == nSize){
            // 首先回来的肯定是种子
            sscanf(pbufMsg, "%4x", &m_nSeed);
            // LOGD("LONGNET Seed is %d", m_nSeed);
            if( 0 != m_nSeed )
            {
                // 连接成功回调
                m_pDelegate->onConnected();
            }
        } else {
            if( 0 == m_nSeed){
                // LOGE("LONGNET Seed is not correct!!!! , need reconnect....");
                break;
            }
            
            // 处理本条消息
            int dlen = 0;
            sscanf(pbufMsg, "%4x", &dlen);
            assert((dlen + 4) == nSize);
            
            unsigned char* buffer = new unsigned char[dlen + 1];
            memset(buffer, 0, dlen);
            memcpy(buffer, pbufMsg + 4, dlen);
            CConnUtil::xorCodeing(m_nSeed + dlen, buffer, dlen);
            
            int debuflen = _MAX_MSGSIZE;
            unsigned char* debuf = new unsigned char[_MAX_MSGSIZE];
            memset(debuf, 0, _MAX_MSGSIZE);
            uncompress((Bytef *)debuf, (uLongf *)&debuflen, (Bytef *)buffer, dlen);
            std::string msg = (char *)debuf;
            
            
            if( NULL != m_pDelegate){
                // LOGM( "LONGNET ReceiveMsg %s", msg.c_str() );

                // 回调出去
                m_pDelegate->onMsg(msg.c_str());
            }
            
            delete []buffer;
            delete []debuf;
        }
    }
}


