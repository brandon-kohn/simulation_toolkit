#pragma once

#ifdef EXACT_STATIC_LIB
    #define EXACT_API 
#else
    #ifdef _WIN32
        #ifdef EXACT_EXPORTS_API
            #define EXACT_API __declspec(dllexport)
        #else
            #define EXACT_API __declspec(dllimport)
        #endif
    #else
        #ifdef EXACT_EXPORTS_API
            #define EXACT_API __attribute__ ((visibility ("default")))
        #else
            #define EXACT_API
        #endif
    #endif
#endif
