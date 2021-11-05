/*
 * @Description: Omnipotent stream pipeline header for Nvidia DeepStream x86 platform.
 * @version: 1.0
 * @Author: Ricardo Lu<shenglu1202@163.com>
 * @Date: 2021-10-27 10:40:45
 * @LastEditors: Ricardo Lu
 * @LastEditTime: 2021-11-05 14:20:32
 */

#ifndef __TS_VIDEO_PIPELINE_H__
#define __TS_VIDEO_PIPELINE_H__

#include "Common.h"

#define TS_LINK_ELEMENT(elem1, elem2) \
    do { \
        if (!elem1 || !elem2) { \
            TS_INFO_MSG_V("WOW! Elments(%p,%p) to link are not all non-null", elem1, elem2); \
            goto done; \
        } \
        if (!gst_element_link (elem1,elem2)) { \
            GstCaps* src_caps = gst_pad_query_caps ((GstPad *) (elem1)->srcpads->data, NULL); \
            GstCaps* sink_caps = gst_pad_query_caps ((GstPad *) (elem2)->sinkpads->data, NULL); \
            TS_ERR_MSG_V ("Failed to link '%s' (%s) and '%s' (%s)", \
                GST_ELEMENT_NAME (elem1), gst_caps_to_string (src_caps), \
                GST_ELEMENT_NAME (elem2), gst_caps_to_string (sink_caps)); \
            goto done; \
        } else { \
            GstCaps* src_caps = (GstPad *) (elem1)->srcpads?gst_pad_query_caps ( \
                (GstPad *) (elem1)->srcpads->data, NULL):NULL; \
            GstCaps* sink_caps = (GstPad *) (elem2)->sinkpads?gst_pad_query_caps ( \
                (GstPad *) (elem2)->sinkpads->data, NULL):NULL; \
            TS_INFO_MSG_V ("Success to link '%s' (%s) and '%s' (%s)", \
                GST_ELEMENT_NAME (elem1), src_caps?gst_caps_to_string (src_caps):"NONE", \
                GST_ELEMENT_NAME (elem2), sink_caps?gst_caps_to_string (sink_caps):"NONE"); \
        } \
    } while (0)

#define TS_ELEM_ADD_PROBE(probe_id, elem, pad, probe_func, probe_type, probe_data) \
    do { \
        GstPad *gstpad = gst_element_get_static_pad (elem, pad); \
        if (!gstpad) { \
            TS_ERR_MSG_V ("Could not find '%s' in '%s'", pad, GST_ELEMENT_NAME(elem)); \
            goto done; \
        } \
        probe_id = gst_pad_add_probe(gstpad, (probe_type), probe_func, probe_data, NULL); \
        gst_object_unref (gstpad); \
    } while (0)

#define TS_ELEM_REMOVE_PROBE(probe_id, elem, pad) \
    do { \
        if (probe_id == 0 || !elem) { \
            break; \
        } \
        GstPad *gstpad = gst_element_get_static_pad (elem, pad); \
        if (!gstpad) { \
            TS_ERR_MSG_V ("Could not find '%s' in '%s'", pad, \
                GST_ELEMENT_NAME(elem)); \
            break; \
        } \
        gst_pad_remove_probe(gstpad, probe_id); \
        gst_object_unref (gstpad); \
    } while (0)


typedef struct _VideoPipelineConfig
{
    /*-------------------------------rtspsrc-------------------------------*/
    std::string  uri_                          { "rtsp://admin:ZKCD1234@10.0.23.227:554" };
    unsigned int rtsp_latency_                 { 0 };
    bool         file_loop_                    { false };
    int          rtsp_reconnect_interval_secs_ { -1 };
    unsigned int rtp_protocols_select_         { 7 };
    unsigned int input_width_                 { 1920 };
    unsigned int input_height_                { 1080 };
    /*----------------------------nveglglessink----------------------------*/
    unsigned int display_x_                    { 0 };
    unsigned int display_y_                    { 0 };
    unsigned int display_width_                { 1920 };
    unsigned int display_height_               { 1080 };
    bool         display_sync_                 { false };
    /*----------------------------nvvideoconvert---------------------------*/
    int          crop_x_                       { -1 };
    int          crop_y_                       { -1 };
    int          crop_width_                   { -1 };
    int          crop_height_                  { -1 };
    std::string  output_format_                { "RGBA" };
    // std::string  buffer_type_                  { "NVMM" };
    unsigned int output_width_                 { 1920 };
    unsigned int output_height_                { 1080 };
    int          output_fps_n_                 { 50 };
    int          output_fps_d_                 { 2 };
    /*----------------------------nvv4l2decoder----------------------------*/
    bool         dec_turbo_                    { true };
    int          dec_skip_frames_              { 0 };
    /*----------------------------nvv4l2encoder----------------------------*/
    bool         rtmp_enable_                  { false };
    unsigned int enc_bitrate_                  { 4000000 };
    unsigned int enc_fps_                      { 25 };
    /*-------------------------------rtmpsink------------------------------*/
    unsigned int rtmp_latency_                 { 0 };
    bool         rtmp_sync_                    { false };
    std::string  rtmp_url_                     { "rtmp://52.81.79.48:1935/live/mask/0" };
}VideoPipelineConfig;

class VideoPipeline
{
public:
    VideoPipeline (const VideoPipelineConfig& config);
    bool Create   (void);
    bool Start    (void);
    bool Pause    (void);
    bool Resume   (void);
    void Destroy  (void);
    void SetCallback (TsPutDataFunc    func, void* args);
    void SetCallback (TsGetResultFunc  func, void* args);
    void SetCallback (TsProcResultFunc func, void* args);
    ~VideoPipeline(void);

private:
    GstElement* CreateURI       (void);
    GstElement* CreateUSBCamera (void);

public:
    VideoPipelineConfig config_;
    GMutex              wait_lock_;
    GCond               wait_cond_;
    GMutex              lock_;
    unsigned long       osd_buffer_probe_;
    unsigned long       source_buffer_probe_;
    unsigned long       sync_before_buffer_probe_;
    unsigned long       sync_buffer_probe_;
    bool                live_source_;
    uint64_t            accumulated_base_;
    uint64_t            prev_accumulated_base_;
    uint64_t            appsinked_frame_count_;
    uint64_t            first_frame_timestamp_;
    uint64_t            last_frame_timestamp_;
    volatile int        sync_count_;

    TsPutDataFunc       put_data_func_;
    void*               put_data_args_;
    TsGetResultFunc     get_result_func_;
    void*               get_result_args_;
    TsProcResultFunc    proc_result_func_;
    void*               proc_result_args_;

    GstElement* pipeline_   { NULL };
    GstElement* source_     { NULL };
    GstElement* decode_     { NULL };
    GstElement* muxer_      { NULL };
    GstElement* tee0_       { NULL };
    GstElement* queue0_     { NULL };
    GstElement* queue1_     { NULL };
    GstElement* transform0_ { NULL };
    // GstElement* capfilter0_ { NULL };
    // GstElement* osd_        { NULL };
    // GstElement* transform2_ { NULL };
    GstElement* encoder_    { NULL };
    GstElement* h264parse_  { NULL };
    GstElement* flvmux_     { NULL };
    GstElement* queue01_    { NULL };
    GstElement* rtmpsink_   { NULL };
    GstElement* display_    { NULL };
    GstElement* transform1_ { NULL };
    GstElement* capfilter1_ { NULL };
    GstElement* appsink_    { NULL };
};

/*
 * gst-launch-1.0 uridecodebin uri=rtsp://admin:ZKCD1234@10.0.23.227:554 ! nvstreammux ! \
 * tee name=t1 ! queue ! nvvideoconvert !  'video/x-raw(memory:NVMM),format=(string)NV12' ! \
 * nvv4l2h264enc ! h264parse ! flvmux ! queue ! rtmpsink location=rtmp://52.81.79.48:1935/live/mask/0 \
 * t1. ! queue ! nvvideoconvert ! 'video/x-raw(memory:NVMM),format=(string)RGBA' ! \
 * appsink
*/

/*
 * gst-launch-1.0 uridecodebin uri=rtsp://admin:ZKCD1234@10.0.23.227:554 ! nvstreammux ! \
 * tee name=t1 ! queue ! nvvideoconvert ! nvdsosd ! nvvideoconvert ! \
 * nvv4l2h264enc ! h264parse ! flvmux ! queue ! rtmpsink location=rtmp://52.81.79.48:1935/live/mask/0 \
 * t1. ! queue ! nvvideoconvert ! 'video/x-raw(memory:NVMM),format=(string)RGBA' ! \
 * appsink
*/

#endif //__TS_VIDEO_PIPELINE_H__
