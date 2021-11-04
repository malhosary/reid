/*
 * @Description: Common AI Algorithm interface header.
 * @version: 1.0
 * @Author: Ricardo Lu<shenglu1202@163.com>
 * @Date: 2021-11-01 09:34:02
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2021-11-04 16:26:24
 */

#ifndef __TS_ALG_INTERFACE_H__
#define __TS_ALG_INTERFACE_H__

#include "Common.h"

extern "C"  void* algInit (const std::string& args);

extern "C"  bool algStart (void* alg);

extern "C"  std::shared_ptr<TsJsonObject> algProc (
    void* alg, const std::shared_ptr<TsGstSample>& data);

extern "C"  bool algCtrl (void* alg, const std::string& cmd);

extern "C"  void algStop (void* alg);

extern "C"  void algFina (void* alg);

extern "C"  bool algSetCb (void* alg, TsPutResult cb, void* args);

#endif //__TS_ALG_INTERFACE_H__