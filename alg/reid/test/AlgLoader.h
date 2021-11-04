/*
 * @Author: your name
 * @Date: 2021-11-04 16:22:31
 * @LastEditTime: 2021-11-04 16:27:00
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /ReID/alg/reid/test/VideoLoader.h
 */

#ifndef __TS_ALG_LOADER_H__
#define __TS_ALG_LOADER_H__  
    
#include <assert.h>
#include <dlfcn.h>

#include "Common.h"

class AlgLoader
{
public:
    AlgLoader (const std::string& path, int mode = RTLD_LAZY) {
        assert (!IsEmpty (path.c_str()));
        
        if (!(lib_handle_ = dlopen (path.c_str(), mode))) {
            TS_ERR_MSG_V ("Failed to open library:%s(%s)",
                path.c_str(), dlerror());
        }
        
    }
    
   ~AlgLoader () {
        if (lib_handle_) {
            dlclose (lib_handle_);
            lib_handle_ = NULL;
        }
    }

    bool  IsValid () const { 
        return NULL != lib_handle_; 
    }
    
    const std::string& GetPath () const { 
        return lib_path_; 
    }

    template <typename API>
    API GetAPI (const char* name)
    {
        assert (!IsEmpty (name));
        
        if (!lib_handle_) {
            return NULL;
        }

        return (API) dlsym (lib_handle_, name);
    }

    template <typename API>
    API GetAPI (const std::string& name)
    {
        return GetAPI<API> (name.c_str());
    }

private:
    bool IsEmpty (const char* str)
    {
        return !str || strlen(str) == 0;
    }

private:
    void*             lib_handle_ { NULL };
    const std::string lib_path_   {      };
};

#endif //__TS_ALG_LOADER_H__

