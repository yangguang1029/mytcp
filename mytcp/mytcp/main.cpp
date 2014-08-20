//
//  main.cpp
//  mytcp
//
//  Created by guangy on 8/18/14.
//  Copyright (c) 2014 iMAC-0003. All rights reserved.
//

#include <iostream>
#include "mytcp.h"
#include <thread>

int main(int argc, const char * argv[])
{

    // insert code here...
    MyTCP* mt = new MyTCP();
    mt->create("127.0.0.1", 12345);
    std::string msg = "12345";
    std::string newmsg = std::to_string(msg.length()) + msg;
    mt->sendMsg(newmsg.c_str(), newmsg.length());
    
//    char s[5];
//    long len = 10;
//    sprintf(s, "%04lx", len);
//    std::cout << s;
    return 0;
}

