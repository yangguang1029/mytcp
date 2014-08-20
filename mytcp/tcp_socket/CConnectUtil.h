//
//  CConnectUtil.h
//  BetCastle
//
//  Created by mac on 13-11-16.
//
//

#ifndef __BetCastle__CConnectUtil__
#define __BetCastle__CConnectUtil__

#include <iostream>

namespace CConnUtil {
    
#ifdef __cplusplus
    extern "C"
    {
#endif
        
        extern unsigned tyrand(unsigned long randseed);
        extern void xorCodeing(int _seed, unsigned char *data, int length);
        extern std::string unzip(unsigned char *data, int len);
        
#ifdef __cplusplus
    }
#endif
    
}

#endif /* defined(__BetCastle__CConnectUtil__) */
