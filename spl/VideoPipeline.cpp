/*
 * @Description: Implement of omnipotent stream pipeline for Nvidia DeepStream x86 platform.
 * @version: 1.1
 * @Author: Ricardo Lu<shenglu1202@163.com>
 * @Date: 2021-10-27 10:40:45
 * @LastEditors: Ricardo Lu
 * @LastEditTime: 2021-11-05 16:39:31
 */

#include "VideoPipeline.h"

static GstPadProbeReturn
cb_osd_buffer_probe (
    GstPad* pad, 
    GstPadProbeInfo* info,
    gpointer user_data)
{
    // TS_INFO_MSG_V ("cb_osd_buffer_probe called");

    VideoPipeline* vp = (VideoPipeline*) user_data;
    GstBuffer* buffer = (GstBuffer*) info->data;

    // sync
    if (info->type & GST_PAD_PROBE_TYPE_BUFFER) {
        g_mutex_lock (&vp->wait_lock_);
        while (g_atomic_int_get (&vp->sync_count_) <= 0)
            g_cond_wait (&vp->wait_cond_, &vp->wait_lock_);
        if (!g_atomic_int_dec_and_test (&vp->sync_count_)) {
            // TS_INFO_MSG_V ("sync_count_:%d", vp->sync_count_);
        }
        g_mutex_unlock (&vp->wait_lock_);
    }

    // osd the result
    if (vp->get_result_func_) {
        const std::shared_ptr<TsJsonObject> results =
            vp->get_result_func_ (vp->get_result_args_);
        if (results && vp->proc_result_func_) {
            vp->proc_result_func_ (buffer, results, vp->proc_result_args_);
        }
    }

    // TS_INFO_MSG_V ("cb_osd_buffer_probe exited");

    return GST_PAD_PROBE_OK;
}

// for test use.
static GstPadProbeReturn
cb_sync_before_buffer_probe (
    GstPad* pad,
    GstPadProbeInfo* info,
    gpointer user_data)
{
    // TS_INFO_MSG_V ("cb_sync_before_buffer_probe called");

    // VideoPipeline* vp = (VideoPipeline*) user_data;
    // GstBuffer* buffer = (GstBuffer*) info->data;

    return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn
cb_sync_buffer_probe (
    GstPad* pad,
    GstPadProbeInfo* info,
    gpointer user_data)
{
    // TS_INFO_MSG_V ("cb_sync_buffer_probe called");

    VideoPipeline* vp = (VideoPipeline*) user_data;
    // GstBuffer* buffer = (GstBuffer*) info->data;

    // sync
    if (info->type & GST_PAD_PROBE_TYPE_BUFFER) {
        g_mutex_lock (&vp->wait_lock_);
        g_atomic_int_inc (&vp->sync_count_);
        g_cond_signal (&vp->wait_cond_);
        g_mutex_unlock (&vp->wait_lock_);
    }

    // TS_INFO_MSG_V ("cb_sync_buffer_probe exited");

    return GST_PAD_PROBE_OK;
}

static gboolean
seek_decoded_file (
    gpointer user_data)
{
    TS_INFO_MSG_V ("seek_decoded_file called");

    VideoPipeline* vp = (VideoPipeline*) user_data;

    gst_element_set_state (vp->pipeline_, GST_STATE_PAUSED);

    if (!gst_element_seek (vp->pipeline_, 1.0, GST_FORMAT_TIME,
        (GstSeekFlags) (GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_FLUSH),
        GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
        GST_WARNING ("Error in seeking pipeline");
    }

    gst_element_set_state (vp->pipeline_, GST_STATE_PLAYING);

    return false;
}

static GstPadProbeReturn
restart_stream_buffer_probe (
    GstPad* pad,
    GstPadProbeInfo* info,
    gpointer user_data)
{
    TS_INFO_MSG_V ("restart_stream_buffer_probe called");

    VideoPipeline* vp = (VideoPipeline*) user_data;
    GstEvent* event = GST_EVENT (info->data);

    if ((info->type & GST_PAD_PROBE_TYPE_BUFFER)) {
        GST_BUFFER_PTS(GST_BUFFER(info->data)) += vp->prev_accumulated_base_;
    }

    if ((info->type & GST_PAD_PROBE_TYPE_EVENT_BOTH)) {
        if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
            g_timeout_add (1, seek_decoded_file, vp);
        }

        if (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT) {
            GstSegment *segment;
            gst_event_parse_segment (event, (const GstSegment **) &segment);
            segment->base = vp->accumulated_base_;
            vp->prev_accumulated_base_ = vp->accumulated_base_;
            vp->accumulated_base_ += segment->stop;
        }

        switch (GST_EVENT_TYPE (event)) {
            case GST_EVENT_EOS:
            case GST_EVENT_QOS:
            case GST_EVENT_SEGMENT:
            case GST_EVENT_FLUSH_START:
            case GST_EVENT_FLUSH_STOP:
                return GST_PAD_PROBE_DROP;
            default:
                break;
        }
    }
    
    return GST_PAD_PROBE_OK;
}

static void
cb_decodebin_child_added (
    GstChildProxy* child_proxy,
    GObject* object,
    gchar* name,
    gpointer user_data)
{
    TS_INFO_MSG_V ("cb_decodebin_child_added called");

    TS_INFO_MSG_V ("Element '%s' added to decodebin", name);
    VideoPipeline* vp = (VideoPipeline*) user_data;

    if (g_strstr_len (name, -1, "nvv4l2decoder") == name) {
        // TS_INFO_MSG_V ("Element skip-frams: %d", vp->config_.dec_skip_frames_);
        g_object_set (object, "skip-frames", vp->config_.dec_skip_frames_, NULL);
        g_object_set (object, "cudadec-memtype", 2, NULL);

        if (g_strstr_len(vp->config_.uri_.c_str(), -1, "file:/") == vp->config_.uri_.c_str() &&
            vp->config_.file_loop_) {
            TS_ELEM_ADD_PROBE (vp->source_buffer_probe_, GST_ELEMENT(object),
                "sink", restart_stream_buffer_probe, (GstPadProbeType) (
                GST_PAD_PROBE_TYPE_EVENT_BOTH | GST_PAD_PROBE_TYPE_EVENT_FLUSH | 
                GST_PAD_PROBE_TYPE_BUFFER), vp);
        }
    }

done:
    return;
}

static void
cb_uridecodebin_source_setup (
    GstElement* object,
    GstElement* arg0,
    gpointer user_data)
{
    VideoPipeline* vp = (VideoPipeline*) user_data;

    TS_INFO_MSG_V ("cb_uridecodebin_source_setup called");

    if (g_object_class_find_property (G_OBJECT_GET_CLASS (arg0), "latency")) {
        TS_INFO_MSG_V ("cb_uridecodebin_source_setup set %d latency", 
            vp->config_.rtsp_latency_);
        g_object_set (G_OBJECT (arg0), "latency", vp->config_.rtsp_latency_, NULL);
    }
}

static void
cb_uridecodebin_pad_added (
    GstElement* decodebin, 
    GstPad* pad, 
    gpointer user_data)
{
    GstCaps* caps = gst_pad_query_caps (pad, NULL);
    const GstStructure* str = gst_caps_get_structure (caps, 0);
    const gchar* name = gst_structure_get_name (str);

    TS_INFO_MSG_V ("cb_uridecodebin_pad_added called");
    TS_INFO_MSG_V ("structure:%s", gst_structure_to_string(str));

    if (!strncmp (name, "video", 5)) {
        VideoPipeline* vp = (VideoPipeline*) user_data;
        GstPad* sinkpad = gst_element_get_request_pad (vp->muxer_, "sink_0");

        if (sinkpad && gst_pad_link (pad, sinkpad) == GST_PAD_LINK_OK) {
            TS_INFO_MSG_V ("Success to link uridecodebin to pipeline");
            GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(vp->pipeline_), 
                GST_DEBUG_GRAPH_SHOW_ALL, "pipeline");
        } else {
            TS_ERR_MSG_V ("Failed to link uridecodebin to pipeline");
        }

        if (sinkpad) {
            gst_object_unref (sinkpad);
        }
    }

    gst_caps_unref (caps);
}

static void
cb_uridecodebin_child_added (
    GstChildProxy* child_proxy,
    GObject* object,
    gchar* name,
    gpointer user_data)
{
    TS_INFO_MSG_V ("cb_uridecodebin_child_added called");

    TS_INFO_MSG_V ("Element '%s' added to uridecodebin", name);
    VideoPipeline* vp = (VideoPipeline*) user_data;

    if (g_strrstr (name, "decodebin") == name) {
        g_signal_connect (G_OBJECT (object), "child-added",
            G_CALLBACK (cb_decodebin_child_added), vp);
    }

    if ((g_strrstr (name, "h264parse") == name) ||
        (g_strrstr (name, "h265parse") == name)) {
        g_object_set (object, "config_-interval", -1, NULL);
    }

    return;
}

GstFlowReturn 
cb_appsink_new_sample (
    GstElement* sink,
    gpointer user_data)
{
    // TS_INFO_MSG_V ("cb_appsink_new_sample called");

    VideoPipeline* vp = (VideoPipeline*) user_data;
    GstSample* sample = NULL;

    g_signal_emit_by_name (sink, "pull-sample", &sample);

    if (sample) {
        // control the frame rate of video submited to analyzer
        if (0 == vp->first_frame_timestamp_) {
            vp->first_frame_timestamp_ = g_get_real_time ();
        } else {
            gint64 period_us_count = g_get_real_time () - vp->first_frame_timestamp_;
            double cur = (double)(vp->appsinked_frame_count_*1000*1000) / (double)(period_us_count);
            double dst = (double)(vp->config_.output_fps_n_) / (double)(vp->config_.output_fps_d_);
            // TS_INFO_MSG_V ("%2.2f, %2.2f, %d/%d", cur, dst, vp->config_.output_fps_n_,
            //    vp->config_.output_fps_d_);

            if (cur > dst) {
                 gst_sample_unref (sample);
                 return GST_FLOW_OK;
            }
        }

        if (vp->put_data_func_) {
            vp->put_data_func_(sample, vp->put_data_args_);
            vp->appsinked_frame_count_++;
        } else {
            gst_sample_unref (sample);
        }
    }

    return GST_FLOW_OK;
}

VideoPipeline::VideoPipeline (
    const VideoPipelineConfig& config)
{
    g_mutex_init (&wait_lock_);
    g_cond_init  (&wait_cond_);
    g_mutex_init (&lock_);
    pipeline_ = NULL;
    config_ = config;
    live_source_ = false;
    osd_buffer_probe_ = 0;
    source_buffer_probe_ = 0;
    sync_before_buffer_probe_ = 0;
    sync_buffer_probe_ = 0;
    accumulated_base_ = 0;
    prev_accumulated_base_ = 0;
    appsinked_frame_count_ = 0;
    first_frame_timestamp_ = 0;
    last_frame_timestamp_ = 0;
    sync_count_ = 0;
    put_data_func_ = NULL;
    put_data_args_ = NULL;
    get_result_func_ = NULL;
    get_result_args_ = NULL;
    proc_result_func_ = NULL;
    proc_result_args_ = NULL;
}

VideoPipeline::~VideoPipeline (void)
{

}

bool
VideoPipeline::Create (void)
{
    GstCapsFeatures* feature;
    GstCaps* caps;

    if (g_strrstr (config_.uri_.c_str(), "file:/")) {
        live_source_ = false;
    } else {
        live_source_ = true;
    }

    if (!(pipeline_ = gst_pipeline_new ("video-pipeline"))) {
        TS_ERR_MSG_V ("Failed to create pipeline named video");
        goto done;
    }

    if (!(source_ = gst_element_factory_make ("uridecodebin", "source"))) {
        TS_ERR_MSG_V ("Failed to create element uridecodebin named source");
        goto done;
    }

    g_object_set (G_OBJECT (source_), "uri", config_.uri_.c_str(), NULL);

    g_signal_connect (G_OBJECT (source_), "source-setup", G_CALLBACK (
        cb_uridecodebin_source_setup), this);
    g_signal_connect (G_OBJECT (source_), "pad-added",    G_CALLBACK (
        cb_uridecodebin_pad_added),    this);
    g_signal_connect (G_OBJECT (source_), "child-added",  G_CALLBACK (
        cb_uridecodebin_child_added),  this);

    gst_bin_add_many (GST_BIN (pipeline_), source_, NULL);

    muxer_ = gst_element_factory_make("nvstreammux", "stream-muxer");
    if (!muxer_) {
        g_printerr("One element could not be created. Exiting.\n");
        return -1;
    }
    gst_bin_add(GST_BIN (pipeline_), muxer_);
    g_object_set(G_OBJECT (muxer_), "width", config_.input_width_, "height",
                 config_.input_height_, "batch-size", 1,
                 "batched-push-timeout", 40000, NULL);

    if (!(tee0_ = gst_element_factory_make ("tee", "tee0"))) {
        TS_ERR_MSG_V ("Failed to create element tee named tee0");
        goto done;
    }

    gst_bin_add_many (GST_BIN (pipeline_), tee0_, NULL);

    if (!(queue0_ = gst_element_factory_make ("queue", "queue0"))) {
        TS_ERR_MSG_V ("Failed to create element queue named queue0");
        goto done;
    }

    gst_bin_add_many (GST_BIN (pipeline_), queue0_, NULL);

    if (!(queue1_ = gst_element_factory_make ("queue", "queue1"))) {
        TS_ERR_MSG_V ("Failed to create element queue named queue1");
        goto done;
    }

    gst_bin_add_many (GST_BIN (pipeline_), queue1_, NULL);

    TS_LINK_ELEMENT (muxer_, tee0_);
    TS_LINK_ELEMENT (tee0_, queue0_);
    TS_LINK_ELEMENT (tee0_, queue1_);

    if (!(transform0_ = gst_element_factory_make ("nvvideoconvert", "transform0"))) {
        TS_ERR_MSG_V ("Failed to create element nvvideoconvert named transform0");
        goto done;
    }

    g_object_set (GST_OBJECT (transform0_), "nvbuf-memory-type", 3, NULL);

    gst_bin_add_many (GST_BIN (pipeline_), transform0_, NULL);

    /* decode frame -> RGBA
    caps = gst_caps_new_simple ("video/x-raw", "format", G_TYPE_STRING, "RGBA",
            "width", G_TYPE_INT, config_.display_width_,
            "height", G_TYPE_INT, config_.display_height_, NULL);
    feature = gst_caps_features_new ("memory:NVMM", NULL);
    gst_caps_set_features (caps, 0, feature);
    if (!(capfilter0_ = gst_element_factory_make("capsfilter", "capfilter0"))) {
        TS_ERR_MSG_V ("Failed to create element capsfilter named capfilter0");
        goto done;
    }

    g_object_set (G_OBJECT(capfilter0_), "caps", caps, NULL);
    gst_caps_unref (caps);

    gst_bin_add_many (GST_BIN (pipeline_), capfilter0_, NULL);
    */

    /* nvdsosd
    if (!(osd_ = gst_element_factory_make ("nvdsosd", "nvosd"))) {
        TS_ERR_MSG_V ("Failed to create element nvdsosd named nvosd");
        goto done;
    }

    gst_bin_add_many (GST_BIN (pipeline_), osd_, NULL);
    */

    /* nveglglessink
    if (!(display_ = gst_element_factory_make ("nveglglessink", "display"))) {
        TS_ERR_MSG_V ("Failed to create element nveglglessink named display");
        goto done;
    }

    g_object_set(G_OBJECT (display_), "window-x", config_.display_x_, NULL);
    g_object_set(G_OBJECT (display_), "window-y", config_.display_y_, NULL);
    g_object_set(G_OBJECT (display_), "window-width", config_.display_width_, NULL);
    g_object_set(G_OBJECT (display_), "window-height", config_.display_height_, NULL);
    g_object_set(G_OBJECT (display_), "force-aspect-ratio", false, NULL);
    g_object_set(G_OBJECT (display_), "sync", config_.display_sync_, NULL);

    gst_bin_add_many (GST_BIN (pipeline_), display_, NULL);
    */

    /* nvvideoconvert: RGBA -> NV12
    if (!(transform2_ = gst_element_factory_make ("nvvideoconvert", "transform2"))) {
        TS_ERR_MSG_V ("Failed to create element nvvideoconvert named transform2_");
        goto done;
    }

    gst_bin_add_many (GST_BIN (pipeline_), transform2_, NULL);
    */

    /*
    if (!(videoconv_ = gst_element_factory_make ("videoconvert", "conv"))) {
        TS_ERR_MSG_V ("Failed to create element videoconvert named conv");
        goto done;
    }

    gst_bin_add_many (GST_BIN (pipeline_), videoconv_, NULL);
    */

    if (!(encoder_ = gst_element_factory_make ("nvv4l2h264enc", "encoder"))) {
        TS_ERR_MSG_V ("Failed to create element nvv4l2h264enc named encoder");
        goto done;
    }

    g_object_set(G_OBJECT (encoder_), "bitrate", config_.enc_bitrate_, NULL);
    g_object_set(G_OBJECT (encoder_), "iframeinterval", config_.enc_fps_, NULL);

    gst_bin_add_many (GST_BIN (pipeline_), encoder_, NULL);

    if (!(h264parse_ = gst_element_factory_make ("h264parse", "parse"))) {
        TS_ERR_MSG_V ("Failed to create element h264parse named parse");
        goto done;
    }

    gst_bin_add_many (GST_BIN (pipeline_), h264parse_, NULL);

    if (!(flvmux_ = gst_element_factory_make ("flvmux", "flvmux0"))) {
        TS_ERR_MSG_V ("Failed to create element flvmux named flvmux0");
        goto done;
    }

    gst_bin_add_many (GST_BIN (pipeline_), flvmux_, NULL);

    if (!(queue01_ = gst_element_factory_make ("queue", "queue01"))) {
        TS_ERR_MSG_V ("Failed to create element queue named queue01");
        goto done;
    }

    gst_bin_add_many (GST_BIN (pipeline_), queue01_, NULL);

    if (!(rtmpsink_ = gst_element_factory_make ("rtmpsink", "rtmpsink0"))) {
        TS_ERR_MSG_V ("Failed to create element rtmpsink named rtmpsink0");
        goto done;
    }

    g_object_set(G_OBJECT (rtmpsink_), "sync", config_.rtmp_sync_, NULL);
    g_object_set(G_OBJECT (rtmpsink_), "location", config_.rtmp_url_.c_str(), NULL);

    gst_bin_add_many (GST_BIN (pipeline_), rtmpsink_, NULL);

    TS_LINK_ELEMENT (queue0_, transform0_);
    // TS_LINK_ELEMENT (transform0_, capfilter0_);
    // TS_LINK_ELEMENT (capfilter0_, transform2_);
    // TS_LINK_ELEMENT (capfilter0_, osd_);
    // TS_LINK_ELEMENT (osd_, display_);
    // TS_LINK_ELEMENT (transform0_, osd_);
    // TS_LINK_ELEMENT (osd_, display_);
    // TS_LINK_ELEMENT (osd_, transform2_);
    // TS_LINK_ELEMENT (transform2_, encoder_);
    // TS_LINK_ELEMENT (capfilter0_, encoder_);
    // TS_LINK_ELEMENT (capfilter0_, videoconv_);
    // TS_LINK_ELEMENT (videoconv_, encoder_);
    TS_LINK_ELEMENT (transform0_, encoder_);
    TS_LINK_ELEMENT (encoder_, h264parse_);
    TS_LINK_ELEMENT (h264parse_, flvmux_);
    TS_LINK_ELEMENT (flvmux_, queue01_);
    TS_LINK_ELEMENT (queue01_, rtmpsink_);

    if (!(transform1_ = gst_element_factory_make ("nvvideoconvert", "transform1"))) {
        TS_ERR_MSG_V ("Failed to create element nvvideoconvert named transform1");
        goto done;
    }

    gst_bin_add_many (GST_BIN ((pipeline_)), transform1_, NULL);

    caps = gst_caps_new_simple ("video/x-raw", "format", G_TYPE_STRING, 
        config_.output_format_.c_str(), "width", G_TYPE_INT, config_.output_width_,
          "height", G_TYPE_INT, config_.output_height_, NULL);
    feature = gst_caps_features_new ("memory:NVMM", NULL);
    gst_caps_set_features (caps, 0, feature);
    if (!(capfilter1_ = gst_element_factory_make("capsfilter", "capfilter1"))) {
        TS_ERR_MSG_V ("Failed to create element capsfilter named capfilter1");
        goto done;
    }

    g_object_set (G_OBJECT(capfilter1_), "caps", caps, NULL);
    gst_caps_unref (caps);

    gst_bin_add_many (GST_BIN (pipeline_), capfilter1_, NULL);

    if (!(appsink_ = gst_element_factory_make ("appsink", "appsink"))) {
        TS_ERR_MSG_V ("Failed to create element appsink named appsink");
        goto done;
    }

    g_object_set (appsink_, "emit-signals", true, NULL);

    g_signal_connect (appsink_, "new-sample", G_CALLBACK (
        cb_appsink_new_sample), this);

    /* test use: appsink -> fakesink
    if (!(appsink_ = gst_element_factory_make ("fakesink", "appsink"))) {
        TS_ERR_MSG_V ("Failed to create element appsink named appsink");
        goto done;
    }
    */

    gst_bin_add_many (GST_BIN (pipeline_), appsink_, NULL);

    TS_LINK_ELEMENT (queue1_,   transform1_);
    TS_LINK_ELEMENT (transform1_, capfilter1_);
    TS_LINK_ELEMENT (capfilter1_, appsink_);

    TS_ELEM_ADD_PROBE (sync_before_buffer_probe_, GST_ELEMENT(transform1_),
        "sink", cb_sync_before_buffer_probe, (GstPadProbeType) (
        GST_PAD_PROBE_TYPE_BUFFER), this);

    TS_ELEM_ADD_PROBE (sync_buffer_probe_, GST_ELEMENT(transform1_),
        "src", cb_sync_buffer_probe, (GstPadProbeType) (
        GST_PAD_PROBE_TYPE_BUFFER), this);

    TS_ELEM_ADD_PROBE (osd_buffer_probe_, GST_ELEMENT(transform0_),
        "src", cb_osd_buffer_probe, (GstPadProbeType) (
        GST_PAD_PROBE_TYPE_BUFFER), this);

    return true;
    
done:
    TS_ERR_MSG_V ("Failed to create video pipeline");

    return false;
}

bool VideoPipeline::Start(void)
{
    if (GST_STATE_CHANGE_FAILURE == gst_element_set_state (pipeline_,
        GST_STATE_PLAYING)) {
        TS_ERR_MSG_V ("Failed to set pipeline to playing state");
        return false;
    }

    return true;
}

bool VideoPipeline::Pause(void)
{
    GstState state, pending;

    TS_INFO_MSG_V ("StartPipeline called");

    if (GST_STATE_CHANGE_ASYNC == gst_element_get_state (
        pipeline_, &state, &pending, 5 * GST_SECOND / 1000)) {
        TS_WARN_MSG_V ("Failed to get state of pipeline");
        return false;
    }

    if (state == GST_STATE_PLAYING) {
        return true;
    } else if (state == GST_STATE_PAUSED) {
        gst_element_set_state (pipeline_, GST_STATE_PLAYING);
        gst_element_get_state (pipeline_, &state, &pending,
            GST_CLOCK_TIME_NONE);
        return true;
    } else {
        TS_WARN_MSG_V ("Invalid state of pipeline(%d)",
            GST_STATE_CHANGE_ASYNC);
        return false;
    }
}

bool VideoPipeline::Resume (void)
{
    GstState state, pending;

    TS_INFO_MSG_V ("StopPipeline called");

    if (GST_STATE_CHANGE_ASYNC == gst_element_get_state (
        pipeline_, &state, &pending, 5 * GST_SECOND / 1000)) {
        TS_WARN_MSG_V ("Failed to get state of pipeline");
        return false;
    }

    if (state == GST_STATE_PAUSED) {
        return true;
    } else if (state == GST_STATE_PLAYING) {
        gst_element_set_state (pipeline_, GST_STATE_PAUSED);
        gst_element_get_state (pipeline_, &state, &pending,
            GST_CLOCK_TIME_NONE);
        return true;
    } else {
        TS_WARN_MSG_V ("Invalid state of pipeline(%d)",
            GST_STATE_CHANGE_ASYNC);
        return false;
    }
}

void VideoPipeline::Destroy (void)
{
    if (pipeline_) {
        gst_element_set_state (pipeline_, GST_STATE_NULL);
        gst_object_unref (pipeline_);
        pipeline_ = NULL;
    }

    g_mutex_clear (&lock_);
    g_mutex_clear (&wait_lock_);
    g_cond_clear  (&wait_cond_);
}

void VideoPipeline::SetCallback (
    TsPutDataFunc cb, 
    void* args)
{
    put_data_func_ = cb;
    put_data_args_ = args;
}

void VideoPipeline::SetCallback (
    TsGetResultFunc func, 
    void* args)
{
    get_result_func_ = func;
    get_result_args_ = args;
}

void VideoPipeline::SetCallback (
    TsProcResultFunc func, 
    void* args)
{
    proc_result_func_ = func;
    proc_result_args_ = args;
}

