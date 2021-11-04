/*
 * @Description: Stream pipelime export APIs.
 * @version: 1.0
 * @Author: Ricardo Lu<shenglu1202@163.com>
 * @Date: 2021-10-27 11:07:52
 * @LastEditors: Ricardo Lu
 * @LastEditTime: 2021-10-28 12:01:05
 */

#ifndef __TS_SPL_INTERFACE_H__
#define __TS_SPL_INTERFACE_H__

#include "Common.h"

typedef struct {
    TsPutDataFunc    putData    { NULL }; // args[0]
    TsGetResultFunc  getResult  { NULL }; // args[1]
    TsProcResultFunc procResult { NULL }; // args[2]
    gpointer         args[3]    { NULL, NULL, NULL };
} SplCallbacks;

extern "C"  void* splInit (const std::string& args);

extern "C"  bool splStart (void* spl);

extern "C"  bool splPause (void* spl);

extern "C"  bool splResume (void* spl);

extern "C"  void splStop (void* spl);

extern "C"  void splFina (void* spl);

extern "C"  bool splSetCb (void* spl, const SplCallbacks& cb);

#endif //__TS_SPL_INTERFACE_H__
