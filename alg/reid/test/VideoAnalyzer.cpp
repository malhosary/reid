/*
 * @Author: your name
 * @Date: 2021-11-04 16:20:58
 * @LastEditTime: 2021-11-04 16:25:50
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /ReID/alg/reid/test/VideoAnalyzer.cpp
 */

#include "VideoAnalyzer.h"

unsigned int VideoAnalyzer::_video_analyzer_id_ = 1;

void VideoAnalyzer::video_analyse_ (void)
{
    TS_INFO_MSG_V ("Thread for video analyzer %d running", 
        video_analyzer_id_);
    
    std::shared_ptr<TsGstSample>  data   = NULL;
    std::shared_ptr<TsJsonObject> result = NULL;

    while (running_) {
        data = NULL; result = NULL;
    
        if (cb_get_func_) {
            if (!(data = cb_get_func_ (cb_get_args_))) {
                //TS_ERR_MSG_V ("Failed to get the data");
                continue;
            }
        }

        if (proc_) {
            result = proc_ (alg_, data);
        }

        if ((!setcb_ || !setcb_ (alg_, NULL, NULL)) && cb_put_func_) {
            if (!cb_put_func_ (result, data, cb_put_args_)) {
                //TS_ERR_MSG_V ("Failed to put the result");
                continue;
            }
        }
    }

    TS_INFO_MSG_V ("Thread for video analyzer %d exited", 
        video_analyzer_id_);
}

VideoAnalyzer::VideoAnalyzer (
    const VideoAnalyzerConfig& config)
{
    video_analyzer_id_ = _video_analyzer_id_++;
    config_ = config;
}

VideoAnalyzer::~VideoAnalyzer (void)
{
    Destroy ();
}

bool
VideoAnalyzer::Create (void)
{
    if (0 == config_.lib_path_.compare("")) {
        init_  = config_.init_;
        start_ = config_.start_;
        proc_  = config_.proc_;
        ctrl_  = config_.ctrl_;
        stop_  = config_.stop_;
        fina_  = config_.fina_;
        setcb_ = config_.setcb_;

        if (!init_ || !start_ || !proc_ || !ctrl_ || !stop_ || 
            !fina_ || !setcb_) {
            TS_ERR_MSG_V ("Config miss some APIs(%p,%p,%p,%p,%p,%p,%p)",
                init_, start_, proc_, ctrl_, stop_, fina_, setcb_);
            return FALSE;
        }
    } else {
        loader_ = new AlgLoader (config_.lib_path_);
        if (!loader_->IsValid ()) {
            return FALSE;
        }

        init_  = loader_->GetAPI<TsAlgInit> ("algInit");
        start_ = loader_->GetAPI<TsAlgStart>("algStart");
        proc_  = loader_->GetAPI<TsAlgProc> ("algProc");
        ctrl_  = loader_->GetAPI<TsAlgCtrl> ("algCtrl");
        stop_  = loader_->GetAPI<TsAlgStop> ("algStop");
        fina_  = loader_->GetAPI<TsAlgFina> ("algFina");
        setcb_ = loader_->GetAPI<TsAlgSetCb>("algSetCb");
        if (!init_ || !start_ || !proc_ || !ctrl_ || !stop_ || 
            !fina_ || !setcb_) {
            TS_ERR_MSG_V ("Failed to load APIs(%p,%p,%p,%p,%p,%p,%p)",
                init_, start_, proc_, ctrl_, stop_, fina_, setcb_);
            return FALSE;
        }
        
        //TS_ERR_MSG_V ("Success to load APIs(%p,%p,%p,%p,%p,%p,%p)",
        //    init_, start_, proc_, ctrl_, stop_, fina_, setcb_);
    }

    if (!(alg_ = init_ (config_.args_))) {
        TS_ERR_MSG_V ("Failed to initialize algorithm instance");
        return FALSE;
    }

    return TRUE;    
}

bool VideoAnalyzer::Start (void)
{
    running_ = TRUE;

    if (start_) {
        if (!start_ (alg_)) {
            return FALSE;
        }
    }
    
    if (!(thread_ = new std::thread (std::bind (
        &VideoAnalyzer::video_analyse_, this)))) {
        TS_ERR_MSG_V ("Failed to new a std::thread object");
        running_ = FALSE;
        return FALSE;
    }

    return TRUE;
}

bool VideoAnalyzer::Control (const std::string& cmd)
{
    if (ctrl_) {
        return ctrl_ (alg_, cmd);
    } else {
        return FALSE;
    }
}

void VideoAnalyzer::Stop (void)
{
    running_ = FALSE;

    if (stop_) {
        stop_ (alg_);
    }

    if (thread_) {
        thread_->join ();
    }
}

void VideoAnalyzer::Destroy (void)
{
    if (fina_) {
        fina_ (alg_);
        alg_= NULL;
    }

    if (thread_) {
        delete thread_;
        thread_ = NULL;
    }

    if (loader_) {
        delete loader_;
        loader_ = NULL;
    }
}

void VideoAnalyzer::SetCallback (
    TsGetData func, 
    void* args)
{
    cb_get_func_ = func;
    cb_get_args_ = args;
}
    
void VideoAnalyzer::SetCallback (
    TsPutResult func, 
    void* args)
{
    cb_put_func_ = func;
    cb_put_args_ = args;
    
    if (setcb_ && setcb_ (alg_, NULL, NULL)) {
        setcb_ (alg_, func, args);
    } 
}

