/*
 * @Description: Stream pipelime export APIs.
 * @version: 1.0
 * @Author: Ricardo Lu<shenglu1202@163.com>
 * @Date: 2021-10-27 11:07:52
 * @LastEditors: Ricardo Lu
 * @LastEditTime: 2021-11-05 17:18:50
 */

#ifndef __TS_SPL_INTERFACE_H__
#define __TS_SPL_INTERFACE_H__

#include "Common.h"

class SplCallbacks{
public:
    SplCallbacks (
        TsPutDataFunc    cbPutData,
        TsGetResultFunc  cbGetResult,
        TsProcResultFunc cbProcResult,
        gpointer         arg0,
        gpointer         arg1,
        gpointer         arg2) {
        putData    = cbPutData   ;
        getResult  = cbGetResult ;
        procResult = cbProcResult;
        args [0]   = arg0;
        args [1]   = arg1;
        args [2]   = arg2;
    }

public:
    TsPutDataFunc    putData;    //args[0]
    TsGetResultFunc  getResult;  //args[1]
    TsProcResultFunc procResult; //args[2]
    gpointer         args[3];
};

extern "C"  void* splInit (const std::string& args);

extern "C"  bool splStart (void* spl);

extern "C"  bool splPause (void* spl);

extern "C"  bool splResume (void* spl);

extern "C"  void splStop (void* spl);

extern "C"  void splFina (void* spl);

extern "C"  bool splSetCb (void* spl, const SplCallbacks& cb);

#endif //__TS_SPL_INTERFACE_H__
