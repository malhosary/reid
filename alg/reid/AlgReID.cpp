/*
 * @Description: Implement of ReID Standard APIs.
 * @version: 1.0
 * @Author: Ricardo Lu<shenglu1202@163.com>
 * @Date: 2021-11-01 09:34:43
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2021-11-04 16:25:59
 */

#include <time.h>
#include <map>

#include <opencv2/opencv.hpp>

#include "AlgInterface.h"
#include "TSObjectReIDPlus.h"

std::map<uint64_t, std::tuple<uint8_t, uint8_t, uint8_t> > color_map;
/*
 * camera_id------->trace_id
 * trace_id-------->trace_path point vector
 */
std::map<uint64_t, std::map<uint64_t, std::vector<std::pair<int, int> > > > trace_map;

typedef struct _AlgConfig {
    std::string config_path_ { "/opt/thundersoft/algs/TSReID.fig" };
    ts::TSDevice device_     { ts::TSDevice::DEVICE_GPU };
    float nms_thresh_        { 0.5 };
    float conf_thresh_       { 0.5 };
    int max_rcg_num_         { 3 };
    float low_dist_          { 0.135 };
    float high_dist_         { 0.16 };
    int max_elem_num_        { 10000 };
} AlgConfig;

typedef struct _AlgCore {
    AlgConfig             cfg_              ;
    ts::TSObjectReIDPlus* alg_      { NULL };
    ts::TSObjectReIDDB*   alg_db_   { NULL };
    TsPutResults cb_put_results_ { NULL };
    void* cb_user_data_        { NULL };
} AlgCore;

static ts::TSDevice string_to_device (std::string& device) 
{
    std::transform(device.begin(), device.end(), device.begin(),
        [](unsigned char ch){ return tolower(ch); }
    );

    if (0 == device.compare("cpu")) {
        return ts::TSDevice::DEVICE_CPU;
    } else if (0 == device.compare("gpu")) {
        return ts::TSDevice::DEVICE_GPU;
    } else if (0 == device.compare("dsp")) {
        return ts::TSDevice::DEVICE_DSP;
    } else { 
        return ts::TSDevice::DEVICE_AUTO;
    }
}

static bool parse_args (AlgConfig& config, const std::string& data)
{
    JsonParser* parser = NULL;
    JsonNode*   root   = NULL;
    JsonObject* object = NULL;
    GError*     error  = NULL;
    bool        ret    = FALSE;

    if (!(parser = json_parser_new ())) {
        TS_ERR_MSG_V ("Failed to new a object with type JsonParser");
        return FALSE;
    }

    if (json_parser_load_from_data (parser, (const gchar *) data.data(), 
        data.length(), &error)) {
        if (!(root = json_parser_get_root (parser))) {
            TS_ERR_MSG_V ("Failed to get root node from JsonParser");
            goto done;
        }

        if (JSON_NODE_HOLDS_OBJECT (root)) {
            if (!(object = json_node_get_object (root))) {
                TS_ERR_MSG_V ("Failed to get object from JsonNode");
                goto done;
            }

            if (json_object_has_member (object, "config-path")) {
                std::string p ((const char*)json_object_get_string_member (
                    object, "config-path"));
                TS_INFO_MSG_V ("\tconfig-path:%s", p.c_str());
                config.config_path_ = p;                
            }

            if (json_object_has_member (object, "device")) {
                std::string d ((const char*)json_object_get_string_member (
                    object, "device"));
                TS_INFO_MSG_V ("\tdevice:%s", d.c_str());
                config.device_ = string_to_device(d); //0-CPU/1-GPU/2-DSP/3-AUTO
            }

            if (json_object_has_member (object, "nms-thresh")) {
                gdouble n = json_object_get_double_member (object, "nms-thresh");
                TS_INFO_MSG_V ("\tnms-thresh:%f", n);
                config.nms_thresh_ = (float)n;
            }

            if (json_object_has_member (object, "conf-thresh")) {
                gdouble c = json_object_get_double_member (object, "conf-thresh");
                TS_INFO_MSG_V ("\tconf-thresh:%f", c);
                config.conf_thresh_ = (float)c;
            }

            if (json_object_has_member (object, "max-rcg-num")) {
                int r = json_object_get_int_member (object, "max-rcg-num");
                TS_INFO_MSG_V ("\tmax-rcg-num:%d", r);
                config.max_rcg_num_ = r;
            }

            if (json_object_has_member (object, "low-distance")) {
                gdouble l = json_object_get_double_member (object, "low-distance");
                TS_INFO_MSG_V ("\tlow-distance:%f", l);
                config.low_dist_ = (float)l;
            }

            if (json_object_has_member (object, "high-distance")) {
                gdouble h = json_object_get_double_member (object, "high-distance");
                TS_INFO_MSG_V ("\thigh-distance:%f", h);
                config.high_dist_ = (float)h;
            }

            if (json_object_has_member (object, "max-elem-num")) {
                int r = json_object_get_int_member (object, "max-elem-num");
                TS_INFO_MSG_V ("\tmax-elem-num:%d", r);
                config.max_elem_num_ = r;
            }
        }
    } else {
        TS_ERR_MSG_V ("Failed to parse json string %s(%s)\n", 
            error->message, data.c_str());
        g_error_free (error);
        goto done;
    }

    ret = TRUE;

done:
    g_object_unref (parser);

    return ret;
}

static JsonObject* results_to_json_object (const std::vector<ts::ReIDData>& results)
{
    JsonObject* result = json_object_new ();
    JsonArray*  jarray = json_array_new ();
    JsonObject* jobject = NULL;

    if (!result || !jarray) {
        TS_ERR_MSG_V ("Failed to new a object with type JsonXyz");
        if (result) json_object_unref (result);
        if (jarray) json_array_unref  (jarray);
        return NULL;
    }

    for (int i = 0; i < (int)results.size(); i ++) {

        if (!(jobject = json_object_new ())) {
            TS_ERR_MSG_V ("Failed to new a object with type JsonObject");
            json_object_unref (result);
            json_array_unref (jarray);
            return NULL;
        }
        
        json_object_set_string_member (jobject, "object-id",
            std::to_string(results[i].object_id).c_str());
        json_object_set_string_member (jobject, "trace-id",
            std::to_string(results[i].trace_id).c_str());
        json_object_set_string_member (jobject, "confidence",
            std::to_string(results[i].confidence).c_str());
        json_object_set_string_member (jobject, "x",
            std::to_string(results[i].x).c_str());
        json_object_set_string_member (jobject, "y",
            std::to_string(results[i].y).c_str());
        json_object_set_string_member (jobject, "width",
            std::to_string(results[i].width).c_str());
        json_object_set_string_member (jobject, "height",
            std::to_string(results[i].height).c_str());
        json_array_add_object_element(jarray, jobject);
    }

    json_object_set_string_member (result, "alg-name", "reid");
    json_object_set_array_member  (result, "alg-result", jarray);
    
    return result;
}

static void results_to_osd_object (
    const std::vector<ts::ReIDData>& results,
    std::vector<TsOsdObject>& osd_object,
    std::vector<TsOsdObject>& osd_point,
    uint64_t camera_id)
{
    std::srand((unsigned)time(NULL));
    
    // results all frome one camera
    for (int i = 0; i < (int)results.size(); i ++) {
        // to-do: object_id to label text; random color seed by object-id
        std::string text = "person_" + std::to_string(results[i].trace_id);
        std::tuple<uint8_t, uint8_t, uint8_t> color;
        if (color_map.find(results[i].trace_id) != color_map.end()) {
            color = color_map[results[i].trace_id];
        } else {
            uint8_t r = rand() % 256;
            uint8_t g = rand() % 256;
            uint8_t b = rand() % 256;
            color = std::make_tuple(r, g, b);
            color_map[results[i].trace_id] = color;
        }

        // object rectangle
        osd_object.push_back (TsOsdObject ((int)results[i].x,
            (int)results[i].y, (int)results[i].width,
            (int)results[i].height, std::get<0>(color), std::get<1>(color), std::get<2>(color),
            0, text, TsObjectType::OBJECT));

        // traced object from which camera
        // to-do: delete lost object trace path point vector
        std::map<uint64_t, std::vector<std::pair<int, int> > > trace;
        if (trace_map.find(camera_id) != trace_map.end()) {
            trace = trace_map[camera_id];
        } else {
            trace_map[camera_id] = trace;
        }

        // traced object path
        // to-do: how to judge trace lost -> trace_id will keep increasing until overflow
        int tmp_x, tmp_y;
        std::pair<int, int> pos;
        if (trace.find(results[i].trace_id) != trace.end()) {
            for (size_t i = 0; i < trace[results[i].trace_id].size(); i++) {
                pos = trace[results[i].trace_id].at(i);
                tmp_x = pos.first;
                tmp_y = pos.second;
                TsOsdObject point (tmp_x, tmp_y, 30, std::get<0>(color),
                    std::get<1>(color), std::get<2>(color), 0, TsObjectType::POINT);
                osd_point.push_back(point);
            }
        }

        // insert new object bbox
        tmp_x = results[i].x + results[i].width / 2;
        tmp_y = results[i].y + results[i].height;
        pos = std::make_pair(tmp_x, tmp_y);
        trace[results[i].trace_id].push_back(pos);
        osd_point.push_back(TsOsdObject (tmp_x, tmp_y, 30, std::get<0>(color),
                    std::get<1>(color), std::get<2>(color), 0, TsObjectType::POINT));
    }
}

RDC_STATE algListener (const std::vector<ts::ReIDData>& reid_vec, void* user_data)
{
    AlgCore* a = (AlgCore*) user_data;

    std::shared_ptr<std::vector<std::shared_ptr<TsJsonObject>>> jos;
    if (!(jos = std::make_shared<std::vector<std::shared_ptr<TsJsonObject>>>())) {
        TS_ERR_MSG_V ("Failed to create a new TsJsonObject with type std::vector");
        return STATE_INVALID_VALUE;
    }

    std::map<uint64_t, std::shared_ptr<std::vector<ts::ReIDData> > > data_map;

    for (auto&& data : reid_vec) {
        if (data_map.end() == data_map.find(data.camera_id)) {
            data_map[data.camera_id] = std::make_shared<std::vector<ts::ReIDData>>();
        }
        data_map[data.camera_id]->push_back(data);
    }

    for (auto it : data_map) {
        std::shared_ptr<TsJsonObject> jo = std::make_shared<TsJsonObject> 
            (results_to_json_object (*(it.second)));
        if (!jo || !jo->GetResult()) {
            TS_ERR_MSG_V ("Failed to new an object with type TsJsonObject"); 
            return false;
        }

        results_to_osd_object (*(it.second), jo->GetOsdObject(),
            jo->GetOsdPoint(), it.first);

        jos->push_back (jo);

    }

    if (a->cb_put_results_ && jos->size() > 0) {
        // std::shared_ptr<TsGstSample> sample = info.sample;

        if (!a->cb_put_results_ (jos, NULL/*sample*/, a->cb_user_data_)) {
            TS_ERR_MSG_V ("Failed to put the result corresponding to sample");
            return STATE_INVALID_VALUE;
        }
    }

    return STATE_SUCCESS;
}

void* algInit (const std::string& args)
{
    TS_INFO_MSG_V ("algInit called");

    AlgCore* a = new AlgCore ();

    if (!a) {
        TS_ERR_MSG_V ("Failed to new a object with type AlgCore");
        return NULL;
    }

    if (!(a->alg_ = new ts::TSObjectReIDPlus())) {
        TS_ERR_MSG_V ("Failed to new a object with type TSObjectReIDPlus");
        goto done;
    }

    // if (!(a->alg_ = new ts::TSObjectReIDDB())) {
    //     TS_ERR_MSG_V ("Failed to new a object with type TSObjectReIDDB");
    //     goto done;
    // }

    TS_INFO_MSG_V ("Algorithm Information: ");
    TS_INFO_MSG_V ("----------------------------------------------------");
    TS_INFO_MSG_V ("%s", a->alg_->getAlgoInfo().c_str());
    TS_INFO_MSG_V ("----------------------------------------------------");

    if (0 != args.compare("")) parse_args(a->cfg_, args);

    if (!a->alg_->initialize (a->cfg_.config_path_, a->cfg_.max_rcg_num_,
        a->cfg_.device_, a->cfg_.device_)) {
        TS_ERR_MSG_V ("Failed to init the algorithm TSObjectReIDPlus");
        goto done;
    }

    a->alg_->setScoreThresh (a->cfg_.conf_thresh_, a->cfg_.nms_thresh_);
    a->alg_->registeronCallBackListener(algListener, a);

    // if (!a->alg_db_->initialize (a->alg_.getFeatureDims(),
    //     a->cfg_.max_elem_num_)) {
    //     TS_ERR_MSG_V ("Failed to init the algorithm TSObjectReIDDB");
    //     goto done;
    // }

    // a->alg_db_.setDistanceThresh(a->cfg_.low_dist_, a->cfg_.high_dist_);

    return (void*) a;

done:
    if (a->alg_) {
        delete a->alg_;
        // delete a->alg_db_;
    }

    delete a;

    return NULL;
}

bool algStart (void* alg)
{
    AlgCore* a = (AlgCore*) alg;

    TS_INFO_MSG_V ("algStart called");

    if (!a->alg_->start()) {
        TS_ERR_MSG_V ("Failed to start the algorithm TSObjectReIDPlus");
        return false;
    }

    return true;
}

std::shared_ptr<std::vector<std::shared_ptr<TsJsonObject>>> algProc (void* alg, 
    const std::shared_ptr<std::vector<std::shared_ptr<TsGstSample>>>& datas)
{
    // TS_INFO_MSG_V ("algProc2 called");

    AlgCore* a = (AlgCore*) alg;
    assert (a);

    for (size_t i = 0; i < datas->size (); i++) {
        gint width, height; 
        GstSample* sample = (*datas)[i]->GetSample();
        GstCaps* caps = gst_sample_get_caps (sample);
        GstStructure* structure = gst_caps_get_structure (caps, 0);
        gst_structure_get_int (structure, "width",  &width );
        gst_structure_get_int (structure, "height", &height);
        std::string format ((char*)gst_structure_get_string (structure, "format"));
        if (0 != format.compare ("RGBA")) {
            TS_ERR_MSG_V ("Invalid format (%s!=RGBA)", format.c_str ());
            return NULL;
        }

        GstMapInfo map;
        GstBuffer* buf = gst_sample_get_buffer (sample);
        gst_buffer_map (buf, &map, GST_MAP_READ);

        // to-do: convert RGBA to RGB through OpenCV
        cv::Mat frame(height, width, CV_8UC4, map.data);
        std::shared_ptr<ts::TSImgData> imgdata = std::make_shared<
                ts::TSImgData>(width, height, TYPE_RGB_U8);
        cv::Mat tmp(imgdata->height(), imgdata->width(), CV_8UC3, imgdata->data());
        cv::cvtColor (frame, tmp, cv::COLOR_RGBA2RGB);
        gst_buffer_unmap (buf, &map);

        uint64_t camera_id = std::stoi((*datas)[i]->GetCameraId());

        a->alg_->feedFrame(imgdata, camera_id);
    }
    return NULL;
}

bool algCtrl(void* alg, const std::string& cmd)
{
    TS_INFO_MSG_V ("algCtrl called");

    return FALSE;
}

void algStop (void* alg)
{
    AlgCore* a = (AlgCore*) alg;
    
    TS_INFO_MSG_V ("algStop called");

    a->alg_->stop();
}

void algFina(void* alg)
{
    AlgCore* a = (AlgCore*) alg;
    
    TS_INFO_MSG_V ("algFina called");

    a->alg_->stop();
    a->alg_->deinitialize();

    // a->alg_db_->deinitialize();
    
    delete a->alg_;
    delete a;
}

bool algSetCb (void* alg, TsPutResults cb, void* args)
{
    AlgCore* a = (AlgCore*) alg;

    if (cb) {
        a->cb_put_results_ = cb;
        a->cb_user_data_ = args;
    }

    return TRUE;
}