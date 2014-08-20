//
//  CConnectUtil.cpp
//  BetCastle
//
//  Created by mac on 13-11-16.
//
//

#include "CConnectUtil.h"
#include "cocos2d.h"
#include <fstream>

namespace CConnUtil{
    
    unsigned tyrand(unsigned long randseed) {
        unsigned long randomNext;
        randomNext = randseed * 1103515245 + 12345;
        return(unsigned)((randomNext/65536) % 32768);
    }
    
    void xorCodeing(int _seed, unsigned char *data, int length) {
        unsigned grandom = _seed;
        for(unsigned i = 0; i < length; i++) {
            grandom = tyrand( grandom );
            data[i] = data[i] ^ ( (unsigned char)( grandom % 255) );
        }
    }
    
    std::string unzip(unsigned char *data, int len) {
        
        std::string msgFile = cocos2d::CCFileUtils::sharedFileUtils()->getWritablePath();
        msgFile.append("/msg.tmp");
        
        std::ofstream ofs;
        ofs.open(msgFile.c_str(), std::ios::out);
    }
}