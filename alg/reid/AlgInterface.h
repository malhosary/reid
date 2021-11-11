/*
 * @Description: Common AI Algorithm interface header.
 * @version: 1.0
 * @Author: Ricardo Lu<shenglu1202@163.com>
 * @Date: 2021-11-01 09:34:02
 * @LastEditors: Ricardo Lu
 * @LastEditTime: 2021-11-11 14:16:17
 */

#ifndef __TS_ALG_INTERFACE_H__
#define __TS_ALG_INTERFACE_H__

#include "Common.h"

extern "C" void* algInit  (const std::string&                        );
extern "C" void* algInit2 (void*, const std::string&                 );
extern "C" bool  algStart (void*                                     );
extern "C" std::shared_ptr<TsJsonObject>
                 algProc  (void*, const std::shared_ptr<TsGstSample>&);
extern "C" std::shared_ptr<std::vector<std::shared_ptr<TsJsonObject>>>
                 algProc2 (void*, const std::shared_ptr<std::vector<
                           std::shared_ptr<TsGstSample>>>&           );
extern "C" bool  algCtrl  (void*, const std::string&                 );
extern "C" void  algStop  (void*                                     );
extern "C" void  algFina  (void*                                     );
extern "C" bool  algSetCb (void*, TsPutResult,  void*                );
extern "C" bool  algSetCb2(void*, TsPutResults, void*                );

#endif //__TS_ALG_INTERFACE_H__