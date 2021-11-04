/*
 * @Author: your name
 * @Date: 2021-11-04 16:21:33
 * @LastEditTime: 2021-11-04 16:24:49
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /ReID/alg/reid/test/VideoAnalyzer.h
 */

#ifndef __TS_VIDEO_ANALYZER_H__
#define __TS_VIDEO_ANALYZER_H__

#include <functional>
#include <thread>

#include "Common.h"
#include "AlgLoader.h"

typedef void* (*TsAlgInit)  (const std::string& args);
typedef std::shared_ptr<TsJsonObject> (*TsAlgProc)   (
    void* alg, const std::shared_ptr<TsGstSample>& data);
typedef bool  (*TsAlgStart) (void* alg);
typedef bool  (*TsAlgCtrl)  (void* alg, const std::string& cmd);
typedef void  (*TsAlgStop)  (void* alg);
typedef void  (*TsAlgFina)  (void* alg);
typedef bool  (*TsAlgSetCb) (void* alg, TsPutResult cb, void* args);

typedef struct _VideoAnalyzerConfig
{
    std::string  lib_path_ { "" };
    std::string  args_   {   "" };
    TsAlgInit    init_   { NULL };
    TsAlgStart   start_  { NULL };
    TsAlgProc    proc_   { NULL };
    TsAlgCtrl    ctrl_   { NULL };
    TsAlgStop    stop_   { NULL };
    TsAlgFina    fina_   { NULL };
    TsAlgSetCb   setcb_  { NULL };
} VideoAnalyzerConfig;

class VideoAnalyzer
{
public:
    VideoAnalyzer (const VideoAnalyzerConfig& config);
    bool Create   (void);
    bool Start    (void);
    bool Control  (const std::string& cmd);
    void Stop     (void);
    void Destroy  (void);
    void SetCallback (TsGetData   func, void* args);
    void SetCallback (TsPutResult func, void* args);
    ~VideoAnalyzer(void);

private:
    void video_analyse_ (void);

public:
    static unsigned int _video_analyzer_id_;
    unsigned int video_analyzer_id_;
    VideoAnalyzerConfig config_ ;
    TsGetData    cb_get_func_{  NULL };
    TsPutResult  cb_put_func_{  NULL };
    void*        cb_get_args_{  NULL };
    void*        cb_put_args_{  NULL };
    TsAlgInit    init_       {  NULL };
    TsAlgStart   start_      {  NULL };
    TsAlgProc    proc_       {  NULL };
    TsAlgCtrl    ctrl_       {  NULL };
    TsAlgStop    stop_       {  NULL };
    TsAlgFina    fina_       {  NULL };
    TsAlgSetCb   setcb_      {  NULL };
    void*        alg_        {  NULL };
    bool         running_    { FALSE };
    std::thread* thread_     {  NULL };
    AlgLoader*   loader_     {  NULL };
};

#endif //__TS_VIDEO_ANALYZER_H__

