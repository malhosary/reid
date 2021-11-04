/*
 * @Author: Ricardo Lu
 * @Date: 2021-11-04 15:46:57
 * @LastEditTime: 2021-11-04 16:25:54
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /ReID/alg/test/main.cpp
 */

#include <unistd.h>
#include <sys/stat.h>

#include <gflags/gflags.h>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <gstnvdsmeta.h>

#include "AlgInterface.h"
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

    json_object_set_string_member (result, "alg-name", "heltdetection");
    json_object_set_array_member  (result, "alg-result", jarray);

    std::shared_ptr<TsJsonObject> jo = std::make_shared<TsJsonObject>(result);
    jo->GetOsdObject().push_back (TsOsdObject(200, 200, 200, 200, 255, 0, 0,
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

    GstMapInfo info;
    NvDsObjectMeta *obj_meta = NULL;
    NvDsMetaList *l_frame = NULL;
    NvDsMetaList *l_obj = NULL;
    NvDsDisplayMeta *display_meta = NULL;
    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(buffer);

    std::vector<TsOsdObject> oos = jobject->GetOsdObject();

    for (size_t i = 0; i < oos.size(); i++) {
        for (l_frame = batch_meta->frame_meta_list; l_frame != NULL;
                l_frame = l_frame->next) {
            NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)(l_frame->data);
            int offset = 0;
            for (l_obj = frame_meta->obj_meta_list; l_obj != NULL; l_obj = l_obj->next)
            {
                obj_meta = (NvDsObjectMeta *)(l_obj->data);
            }
            display_meta = nvds_acquire_display_meta_from_pool(batch_meta);

            /* Parameters to draw text onto the On-Screen-Display */
            NvOSD_TextParams *txt_params = &display_meta->text_params[0];
            display_meta->num_labels = 1;
            txt_params->display_text = (char *)g_malloc0(64);
            snprintf(txt_params->display_text, 64, "%s", "test");

            txt_params->x_offset = oos[i].x_;
            txt_params->y_offset = oos[i].y_ + 12;

            txt_params->font_params.font_name = (char*)"Mono";
            txt_params->font_params.font_size = 10;
            txt_params->font_params.font_color.red = 1.0;
            txt_params->font_params.font_color.green = 0.0;
            txt_params->font_params.font_color.blue = 0.0;
            txt_params->font_params.font_color.alpha = 1.0;

            // txt_params->set_bg_clr = 1;
            // txt_params->text_bg_clr.red = 0.0;
            // txt_params->text_bg_clr.green = 0.0;
            // txt_params->text_bg_clr.blue = 0.0;
            // txt_params->text_bg_clr.alpha = 1.0;

            NvOSD_RectParams *rect_params = &display_meta->rect_params[0];
            display_meta->num_rects = 1;

            rect_params->left = oos[i].x_;
            rect_params->top = oos[i].y_;
            rect_params->width = oos[i].w_;
            rect_params->height = oos[i].h_;
            rect_params->border_width= 10;
            rect_params->border_color.red = 1.0;
            rect_params->border_color.green = 0.0;
            rect_params->border_color.blue = 0.0;
            rect_params->border_color.alpha = 1.0;

            NvOSD_CircleParams &cparams = display_meta->circle_params[0];
            cparams.xc = oos[i].x_ + oos[i].w_ / 2;
            cparams.yc = oos[i].y_;
            cparams.radius = 8;
            cparams.circle_color = NvOSD_ColorParams{244, 67, 54, 1};
            cparams.has_bg_color = 1;
            cparams.bg_color = NvOSD_ColorParams{0, 255, 0, 1};

            nvds_add_display_meta_to_frame(frame_meta, display_meta);
        }
    }
}

int main(int argc, char* argv[])
{
    google::ParseCommandLineFlags (&argc, &argv, true);
    gst_init (&argc, &argv);

    GMainLoop* ml = NULL;
    
    DataDoubleCache<TsJsonObject>* dd = new DataDoubleCache<TsJsonObject> (NULL);
    // DataDoubleCache<TsGstSample>* dm = new DataDoubleCache<TsGstSample> (NULL);
    DataMailbox<TsGstSample>* dm = new DataMailbox<TsGstSample> (DISCARD_OLDEST, -1);

    SplCallbacks splcbs;
    void* spl;

    if (!(spl = splInit (FLAGS_cfg))) {
        TS_ERR_MSG_V ("Failed to initialize stream pipeline instance");
        goto done;
    }

    splcbs.putData = putData;
    splcbs.getResult = getResult;
    splcbs.procResult = procResult;
    splcbs.args[0] = dm;
    splcbs.args[1] = dd;
    splcbs.args[2] = NULL;
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