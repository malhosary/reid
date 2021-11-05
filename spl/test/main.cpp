/*
 * @Description: Test program of omnipotent stream pipeline.
 * @version: 1.0
 * @Author: Ricardo Lu<shenglu1202@163.com>
 * @Date: 2021-10-27 11:09:55
 * @LastEditors: Ricardo Lu
 * @LastEditTime: 2021-11-05 17:26:39
 */

#include <unistd.h>
#include <sys/stat.h>

#include <gflags/gflags.h>
#include <opencv2/opencv.hpp>
#include <gstnvdsmeta.h>
#include <nvbufsurface.h>

#include "SplInterface.h"
#include "DataDoubleCache.h"
#include "DataMailbox.h"
#include "osdyuv.h"

static bool validateCfg(const char* name, const std::string& value)
{
    if (!value.compare("")) {
        TS_ERR_MSG_V ("Config file required!");
        return false;
    }

    struct stat statbuf;
    if (!stat(value.c_str(), &statbuf)) {
        TS_INFO_MSG_V ("Found config file: %s", value.c_str());
        return true;
    }

    TS_ERR_MSG_V ("Invalid config file.");
    return false;
}

DEFINE_string(cfg, "./config.json", "config file in json format.");
DEFINE_validator(cfg, &validateCfg);

static bool putData(GstSample* sample, void* user_data)
{
    TS_INFO_MSG_V ("putData called");

    DataMailbox<TsGstSample>* dm = (DataMailbox<TsGstSample>*)user_data;
    // DataDoubleCache<TsGstSample>* dm = (DataDoubleCache<TsGstSample>*)user_data;

    std::shared_ptr<TsGstSample> gsample = 
        std::make_shared<TsGstSample> (sample, g_get_real_time(),
                                        "0001", "test-pipeline");

    if (!dm->Post(gsample)) {
        TS_WARN_MSG_V ("Failed to post new TsGstSample into datamanager");
        return false;
    }

    return true;  
}

static std::shared_ptr<TsGstSample> getData(void* user_data)
{
    TS_INFO_MSG_V ("getData called");

    DataMailbox<TsGstSample>* dm = (DataMailbox<TsGstSample>*)user_data;

    std::shared_ptr<TsGstSample> gsample =  NULL;

    if (!dm->Pend(gsample, 100)) {
        TS_WARN_MSG_V ("Failed to pend a TsGstSample from data manager");
        return NULL;
    }

    GstSample* sample = gsample->GetSample();
    gst_sample_unref(sample);
    return gsample;
}

bool putResult(std::shared_ptr<TsJsonObject>& result,
    const std::shared_ptr<TsGstSample>& data, void* user_data)
{
    TS_INFO_MSG_V ("putResult called");

    DataDoubleCache<TsJsonObject>* dd = (DataDoubleCache<TsJsonObject>*)user_data;

    if (result) {
        uuid_t uuid; uuid_generate_random (uuid);
        result->Update (uuid, std::string("result"),
            data.get() ? data->GetTimestamp():g_get_real_time(),
            std::string("edge"), std::string("cloud"),
            "0001", std::string(".jpg"));
        result->Print ();

        if (!dd->Post(result)) {
            TS_WARN_MSG_V ("Failed to post a TsJsonObject to result manager");
            return false;
        }
    }

    return true;
}

static std::shared_ptr<TsJsonObject> getResult(void* user_data)
{
    TS_INFO_MSG_V ("getResult called");

    DataDoubleCache<TsJsonObject>* dd = (DataDoubleCache<TsJsonObject>*)user_data;

    JsonObject* result = json_object_new ();
    JsonArray*  jarray = json_array_new ();
    JsonObject* jobject = NULL;
    
    if (!result || !jarray) {
        TS_ERR_MSG_V ("Failed to new a object with type JsonXyz");
        return NULL;
    }

    if (!(jobject = json_object_new ())) {
        TS_ERR_MSG_V ("Failed to new a object with type JsonObject");
        json_array_unref (jarray);
        return NULL;
    }

    json_object_set_string_member(jobject, "type", "test");
    json_object_set_string_member(jobject, "x", "100");
    json_object_set_string_member(jobject, "y", "100");
    json_object_set_string_member(jobject, "width", "1720");
    json_object_set_string_member(jobject, "height", "880");
    json_array_add_object_element(jarray, jobject);

    json_object_set_string_member (result, "alg-name", "reid");
    json_object_set_array_member  (result, "alg-result", jarray);

    std::shared_ptr<TsJsonObject> jo = std::make_shared<TsJsonObject>(result);
    jo->GetOsdObject().push_back (TsOsdObject(100, 100, 1720, 880, 255, 0, 0,
            0, "Test", TsObjectType::OBJECT));

    // if (!dd->Pend(jobject, 0)) {
    //     return NULL;
    // }

    return jo;
}

static void procResult (GstBuffer* buffer,
    const std::shared_ptr<TsJsonObject>& jobject, gpointer user_data)
{
    TS_INFO_MSG_V ("osdResult called");

    NvBufSurface *surface = NULL;
    NvDsMetaList *l_frame = NULL;
    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(buffer);

    // VideoPipeline* vp = (VideoPipeline*) user_data;

    GstMapInfo info;
    if (!gst_buffer_map(buffer, &info,
        (GstMapFlags) (GST_MAP_READ | GST_MAP_WRITE))) {
        TS_WARN_MSG_V ("WHY? WHAT PROBLEM ABOUT SYNC?");
        gst_buffer_unmap(buffer, &info);
        return;
    }

    surface = (NvBufSurface *) info.data;
    TS_INFO_MSG_V ("surface type: %d", surface->memType);

    uint32_t frame_width, frame_height, frame_pitch;
    for (l_frame = batch_meta->frame_meta_list; l_frame != NULL;
            l_frame = l_frame->next) {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)(l_frame->data);
        frame_width = surface->surfaceList[frame_meta->batch_id].width;
        frame_height = surface->surfaceList[frame_meta->batch_id].height;
        frame_pitch = surface->surfaceList[frame_meta->batch_id].pitch;

        if (NvBufSurfaceMap (surface, 0, 0, NVBUF_MAP_READ_WRITE)) {
            TS_ERR_MSG_V ("NVMM map failed.");
            return ;
        }

        // cv::Mat tmpMat(frame_height, frame_width, CV_8UC4,
        //             surface->surfaceList[frame_meta->batch_id].mappedAddr.addr[0],
        //             frame_picth);

        // std::vector<TsOsdObject> oos = jobject->GetOsdObject();
        // for (size_t i = 0; i < oos.size(); i++) {
        //     if (oos[i].x_>=0 && oos[i].w_>0 && (oos[i].x_+oos[i].w_)<frame_width &&
        //         oos[i].y_>=0 && oos[i].h_>0 && (oos[i].y_+oos[i].h_)<frame_height) {

        //         cv::Rect rect (oos[i].x_, oos[i].y_, oos[i].w_, oos[i].h_);
        //         cv::Scalar color (oos[i].r_, oos[i].g_, oos[i].b_);

        //         cv::rectangle(tmpMat, rect, color, 20);
        //     }
        // }

        YUVImgInfo m_YUVImgInfo;
        m_YUVImgInfo.imgdata = reinterpret_cast<uint8_t*>
            (surface->surfaceList[frame_meta->batch_id].mappedAddr.addr[0]);
        m_YUVImgInfo.width = frame_pitch;
        m_YUVImgInfo.height = frame_height;
        m_YUVImgInfo.yuvType = TYPE_YUV420SP_NV12;

        std::vector<TsOsdObject> oos = jobject->GetOsdObject();

        for (size_t i = 0; i < oos.size(); i++) {
            if (oos[i].x_>=0 && oos[i].w_>0 && (oos[i].x_+oos[i].w_)<frame_width &&
                oos[i].y_>=0 && oos[i].h_>0 && (oos[i].y_+oos[i].h_)<frame_height) {
                unsigned char R = oos[i].r_, G = oos[i].g_, B = oos[i].b_;
                unsigned char Y = 0.257*R + 0.504*G + 0.098*B +  16;
                unsigned char U =-0.148*R - 0.291*G + 0.439*B + 128;
                unsigned char V = 0.439*R - 0.368*G - 0.071*B + 128;
                YUVPixColor m_Color = {Y, U, V};

                YUVRectangle m_Rect;
                m_Rect.x = oos[i].x_;
                m_Rect.y = oos[i].y_;
                m_Rect.width = oos[i].w_;
                m_Rect.height = oos[i].h_;

                drawRectangle(&m_YUVImgInfo, m_Rect, m_Color, 20);
            }
        }
    }

    NvBufSurfaceUnMap (surface, 0, 0);
    gst_buffer_unmap(buffer, &info);
}

int main(int argc, char* argv[])
{
    google::ParseCommandLineFlags (&argc, &argv, true);
    gst_init (&argc, &argv);

    GMainLoop* ml = NULL;
    
    DataDoubleCache<TsJsonObject>* dd = new DataDoubleCache<TsJsonObject> (NULL);
    // DataDoubleCache<TsGstSample>* dm = new DataDoubleCache<TsGstSample> (NULL);
    DataMailbox<TsGstSample>* dm = new DataMailbox<TsGstSample> (DISCARD_OLDEST, -1);

    SplCallbacks splcbs (putData, getResult, procResult, dm, dd, nullptr);
    void* spl;

    if (!(spl = splInit (FLAGS_cfg))) {
        TS_ERR_MSG_V ("Failed to initialize stream pipeline instance");
        goto done;
    }

    splSetCb (spl, splcbs);

    if (!splStart (spl)) {
        TS_ERR_MSG_V ("Failed to start stream pipeline instance");
        goto done;
    }

    if (!(ml = g_main_loop_new (NULL, false))) {
        TS_ERR_MSG_V ("Failed to new a object with type GMainLoop");
        goto done;
    }

    sleep(5);
    while (1) {
        getData(dm);
        usleep (25 * 1000);
    }

    g_main_loop_run (ml);
done:
    if (ml) g_main_loop_unref (ml);

    if (spl) {
        splStop (spl);
        splFina (spl);
        spl = NULL;
    }

    if (dm) delete dm;
    if (dd) delete dd;

    return 0;
}