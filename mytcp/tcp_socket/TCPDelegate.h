//
//  TCPDelegate.h
//  framework
//
//  Created by weitongming on 13-12-10.
//
//

#ifndef TestJavascript_TCPDelegate_h
#define TestJavascript_TCPDelegate_h

// tcp连接的回调接口(c++层面的)
class TCPDelegate
{
public:
    virtual ~TCPDelegate() {};
    
    // 连接已经建立
    virtual void onConnected() = 0;
    
    virtual void onClosed() = 0;
    
    // 连接出错
    virtual void onError( int errCode ) = 0;
    
    // 收到消息
    virtual void onMsg(const char* msg) = 0;
    
    virtual void onReconnect() = 0;
};

#endif
