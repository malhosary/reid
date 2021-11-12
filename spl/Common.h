/*
 * @Description: AIRunner common resource header.
 * @version: 1.2
 * @Author: Ricardo Lu<shenglu1202@163.com>
 * @Date: 2021-10-27 10:40:30
 * @LastEditors: Ricardo Lu
 * @LastEditTime: 2021-11-11 14:33:52
 */

#ifndef __TS_COMMON_H__
#define __TS_COMMON_H__

#include <assert.h>
#include <memory>
#include <string>
#include <vector>

#include <json-glib/json-glib.h>
#include <uuid/uuid.h>
#include <gst/gst.h>

#define TS_ERR_MSG_V(msg, ...)  \
    g_print("** ERROR: <%s:%s:%d>: " msg "\n", \
        __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#define TS_INFO_MSG_V(msg, ...) \
    g_print("** INFO:  <%s:%s:%d>: " msg "\n", \
        __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#define TS_WARN_MSG_V(msg, ...) \
    g_print("** WARN:  <%s:%s:%d>: " msg "\n", \
        __FILE__, __func__, __LINE__, ##__VA_ARGS__)

class TsGstBuffer 
{
public:
    TsGstBuffer (
        GstBuffer* buffer) {
        if (buffer) {
            gst_buffer_ref (buffer);
            buffer_  = buffer;
        }
    }

   ~TsGstBuffer () {
        if (buffer_) {
            gst_buffer_unref (buffer_);
            buffer_ = nullptr;
        }
    }

    GstBuffer* GetBuffer (void) {
        return buffer_;
    }

    GstBuffer* RefBuffer (void) {
        if (buffer_) {
            gst_buffer_ref (buffer_);
        }

        return buffer_;
    }

private:
    GstBuffer* buffer_ { nullptr };
};

class TsGstSample 
{
public:
    TsGstSample (
        GstSample*         sample,
        gint64             timestamp,
        const std::string& cameraid,
        const std::string& userdata = ""
    ) : sample_    (sample   ),
        timestamp_ (timestamp),
        camera_id_ (cameraid ),
        user_data_ (userdata ),
        buffer_    (nullptr  ),
        format_    (nullptr  ),
        rows_      (0        ),
        cols_      (0        ),
        fpsn_      (0        ),
        fpsd_      (0        ) {
    }

   ~TsGstSample () {
        if (sample_) {
            gst_sample_unref (sample_);
            sample_ = nullptr;
        }
    }

    GstSample* GetSample (void) {
        return sample_;
    }

    GstSample* RefSample (void) {
        return gst_sample_ref (sample_);
    }

    GstBuffer* GetBuffer (
        int&         width,
        int&         height,
        std::string& format) {
        if (!buffer_) {
            GstCaps* caps = gst_sample_get_caps (sample_);
            GstStructure* structure = gst_caps_get_structure (caps, 0);
            gst_structure_get_int (structure, "width",  &cols_);
            gst_structure_get_int (structure, "height", &rows_);
            gst_structure_get_fraction (structure, "framerate", &fpsn_, &fpsd_);
            format_ = gst_structure_get_string (structure, "format");
            buffer_ = gst_sample_get_buffer (sample_);
        }

        width  = cols_;
        height = rows_;
        format = format_;
        return buffer_;
    }

    GstBuffer* RefBuffer (
        int&         width,
        int&         height,
        std::string& format) {
        return gst_buffer_ref (GetBuffer (width, height, format));
    }

    gint64 GetTimestamp (void) {
        return timestamp_;
    }

    const std::string& GetCameraId (void) {
        return camera_id_;
    }

    const std::string& GetUserData (void) {
        return user_data_;
    }

private:
    //-------------------------------------
    GstSample*      sample_    { nullptr };
    GstBuffer*      buffer_    { nullptr };
    //-------------------------------------
    const gchar*    format_    { nullptr };
    gint            cols_      { 0       },
                    rows_      { 0       },
                    fpsn_      { 0       },
                    fpsd_      { 0       };
    gint64          timestamp_ { 0       };
    std::string     camera_id_ { ""      };
    //-------------------------------------
    // user_data_: splname in the config  ;
    std::string     user_data_ { ""      };
    //-------------------------------------
};

typedef enum _TsObjectType {
    OBJECT,
    ROI
} TsObjectType;

class TsOsdObject
{
public:
    TsOsdObject (
        int           x,
        int           y,
        int           w,
        int           h,
        unsigned char r,
        unsigned char g,
        unsigned char b,
        unsigned int  reserved,
        const std::string& text,
        TsObjectType type = TsObjectType::OBJECT
    ) : type_     (type    ),
        reserved_ (reserved),
        x_        (x       ),
        y_        (y       ),
        w_        (w       ),
        h_        (h       ),
        text_     (text    ),
        r_        (r       ),
        g_        (g       ),
        b_        (b       ) {
    }

    bool HaveRect (void) {
        return x_ >= 0 && y_ >= 0 && w_ >= 0 && h_ >= 0;
    }

    bool HavePos (void) {
        return x_ >= 0 && y_ >= 0 ;
    }

    bool HaveText (void) {
        return text_.length () > 0;
    }

    bool IsRoi (void) {
        return type_ == TsObjectType::ROI;
    }

    bool IsObject (void) {
        return type_ == TsObjectType::OBJECT;
    }

public:
    //-----------------------------------------------
    TsObjectType  type_     { TsObjectType::OBJECT };
    //-----------------------------------------------
    unsigned int  reserved_ { 0                    };
    //-----------------------------------------------
    int           x_        { -1                   },
                  y_        { -1                   },
                  w_        { -1                   },
                  h_        { -1                   };
    //-----------------------------------------------
    std::string   text_     { nullptr              };
    //-----------------------------------------------
    unsigned char r_        { 255                  },
                  g_        { 0                    },
                  b_        { 0                    };
    //-----------------------------------------------
};

class TsJsonObject 
{
public:
    TsJsonObject (
        JsonObject* result = nullptr,
        size_t size = 2) {
        result_ = result;
    }

   ~TsJsonObject () {
        if (object_) {
            json_object_unref (object_);
        } 
        if (result_) {
	        json_object_unref (result_);
        }
    }

    bool Update (
        const uuid_t&      uuid,
        const std::string& data_type,
        gint64             timestamp,
        const std::string& source,
        const std::string& dest,
        const std::string& camera_id,
        const std::string& picture_type,
        const std::string& userdata = "") {
        if (object_) return true;
        if (!(object_ = json_object_new ())) {
            return false;
        }

        char uuids[UUID_STR_LEN + 1];
        uuid_unparse (uuid, uuids);
        uuid_         = uuids;
        camera_id_    = camera_id;
        timestamp_    = timestamp;
        picture_type_ = picture_type;
        SetUserData  (userdata, 0);

        json_object_set_string_member (result_, 
            (gchar*)("camera-id"),    (gchar*)(camera_id.c_str()));
        json_object_set_string_member (object_, 
            (gchar*)("type"),         (gchar*)(data_type.c_str()));
        json_object_set_int_member    (object_, 
            (gchar*)("timestamp"),    (gint64)(timestamp));
        json_object_set_string_member (object_, 
            (gchar*)("uuid"),         (gchar*)(uuids));
        json_object_set_string_member (object_,
            (gchar*)("source"),       (gchar*)(source.c_str()));
        json_object_set_string_member (object_, 
            (gchar*)("destination"),  (gchar*)(dest.c_str()));

        if (result_) {
            json_object_set_object_member (object_, 
                (gchar*)("data"), result_);
            result_ = nullptr;
        }

        JsonNode *root = json_node_new (JSON_NODE_OBJECT);
        if (root) {
            json_node_set_object (root, object_);
            char* message = json_to_string (root, true);
            json_node_free (root);
            if (message) {
                message_ = message;
                g_free (message);
            }
        }

        return true;
    }

    void Print (void) {
        JsonNode *root = json_node_new (JSON_NODE_OBJECT);
        if (root) {
            json_node_set_object (root, object_?object_:result_);
            char* message = json_to_string (root, true);
            json_node_free (root);

            if (message) {
                TS_INFO_MSG_V ("Message: \n%s", message);
                g_free (message);
            }
        }
    }

    std::vector<TsOsdObject>& GetOsdObject (void) {
        return osd_;
    }

    std::vector<unsigned char>& GetPictureBuffer (void) {
        return picture_data_;
    }

    const std::vector<unsigned char>& GetPictureData (void) {
        return picture_data_;
    }

    const std::string& GetMessage (void) {
        return message_;
    }

    gint64 GetTimestamp (void) {
        return timestamp_;
    }

    const std::string& GetUuid (void) {
        return uuid_;
    }

    const std::string& GetPictureType (void) {
        return picture_type_;
    }

    const std::string& GetCameraId (void) {
        return camera_id_;
    }

    const std::string& GetUserData (
        size_t index = 0) {
        if (index > user_datas_.size () - 1) {
            user_datas_.resize (index + 1, "");
        }

        return user_datas_[index];
    }

    void SetUserData (
        const std::string& userdata = "",
        size_t index = 0) {
        if (index >= user_datas_.size ()) {
            user_datas_.resize (index + 1, "");
        }

        user_datas_[index] = userdata;
    }

    const JsonObject* GetResult (void) {
        return result_;
    }

    bool GetSnapPicture (void) {
        return snap_picture_;
    }

    void SetSnapPicture (
        bool snap) {
        snap_picture_ = snap;
    }

    void Clear (void) {
        osd_.clear ();
    }

    void Merge (
        const std::shared_ptr<TsJsonObject>& from) {
        std::vector<TsOsdObject>& osd = from->GetOsdObject ();
        for (size_t i = 0; i < osd.size (); i++) {
            osd_.push_back (osd[i]);
        }
    }

private:
    //---------------------------------------------------
    JsonObject*                object_       { nullptr };
    JsonObject*                result_       { nullptr };
    //---------------------------------------------------
    std::string                message_      { "{}"    };
    std::vector<TsOsdObject>   osd_          {         };
    std::vector<unsigned char> picture_data_ {         };
    //---------------------------------------------------
    bool                       snap_picture_ { true    };
    gint64                     timestamp_    { 0       };
    std::string                uuid_         { ""      };
    std::string                camera_id_    { ""      };
    std::string                picture_type_ { ""      };
    //---------------------------------------------------
    // user_datas_[0]: splname in the config            ;
    // user_datas_[1]: algname in the config            ;
    std::vector<std::string>   user_datas_   {         };
    //---------------------------------------------------
};


typedef std::shared_ptr<TsGstSample> (*TsGetData)(void*);

typedef bool (*TsPutResult)(
    std::shared_ptr<TsJsonObject>&,
    const std::shared_ptr<TsGstSample>&,
    void*
);

typedef std::shared_ptr<std::vector<std::shared_ptr<TsGstSample>>> (*TsGetDatas)(void*);

typedef bool (*TsPutResults)(
    std::shared_ptr<std::vector<std::shared_ptr<TsJsonObject>>>&,
    const std::shared_ptr<std::vector<std::shared_ptr<TsGstSample>>>&,
    void*
);

typedef bool (*TsPutDataFunc)(GstSample*, void*);

typedef std::shared_ptr<TsJsonObject> (*TsGetResultFunc)(void*);

typedef void (*TsProcResultFunc)(
    GstBuffer* buffer,
    const std::shared_ptr<TsJsonObject>&,
    void*
);

#endif //__TS_COMMON_H__

