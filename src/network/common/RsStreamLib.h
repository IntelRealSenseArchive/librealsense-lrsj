// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 Intel Corporation. All Rights Reserved.

#pragma once

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#include <option.h>
#include <software-device.h>
#include <librealsense2/hpp/rs_internal.hpp>
#include <librealsense2/rs.hpp>

#include <list>
#include <sstream>

using SoftSensor      = std::shared_ptr<rs2::software_sensor>;

class slib {
public:
    slib() {};
   ~slib() {};

    static std::string print_profile(rs2::stream_profile profile) {
        std::stringstream ss;
        ss << std::setw(10) << profile.stream_type() << std::setw(2);
        if (profile.stream_index()) ss << profile.stream_index();
        else ss << "";
        ss << std::setw(15) << rs2_format_to_string(profile.format());

        std::stringstream vss;
        if (profile.is<rs2::video_stream_profile>()) {
            rs2::video_stream_profile vsp = profile.as<rs2::video_stream_profile>();
            vss << vsp.width() << "x" << vsp.height();
        }
        
        ss << std::setw(10) << vss.str() << ":" << profile.fps() << "\t" << (profile.is_default() ? '*' : ' ');
        return ss.str();
    };

    static std::string print_stream(rs2_video_stream* pstream) {
        rs2_motion_stream* pmstream = (rs2_motion_stream*)pstream;

        std::stringstream ss;
        std::stringstream vss;

        switch (pstream->type) {
        case RS2_STREAM_DEPTH    :
        case RS2_STREAM_COLOR    :
        case RS2_STREAM_INFRARED :
        case RS2_STREAM_FISHEYE  :
            ss << std::setw(10) << pstream->type << std::setw(2);
            if (pstream->index) ss << pstream->index;
            else ss << "";
            ss << std::setw(15) << rs2_format_to_string(pstream->fmt);
            vss << pstream->width << "x" << pstream->height;
            ss << std::setw(10) << vss.str() << ":" << pstream->fps;
            break;
        case RS2_STREAM_GYRO     :
        case RS2_STREAM_ACCEL    :
            ss << std::setw(10) << pmstream->type << std::setw(2);
            if (pmstream->index) ss << pmstream->index;
            else ss << "";
            ss << std::setw(15) << rs2_format_to_string(pmstream->fmt);        
            ss << std::setw(10) << vss.str() << ":" << pmstream->fps;
            break;
        default:
            ss << "Unknown stream type";
        }        
        return ss.str();
    };

    static uint64_t profile2key(rs2::stream_profile profile, rs2_format format = RS2_FORMAT_ANY) {
        convert_t t;
        t.key = 0;

        t.values.def    = profile.is_default();
        t.values.type   = profile.stream_type();  
        t.values.index  = profile.stream_index(); 
        t.values.uid    = profile.unique_id();    
        t.values.fps    = profile.fps();
        if (format == RS2_FORMAT_ANY) {
            t.values.format = profile.format();
        } else {
            t.values.format = format;
        }

        if (profile.is<rs2::video_stream_profile>()) {
            rs2::video_stream_profile vsp = profile.as<rs2::video_stream_profile>();
            t.values.width  = vsp.width();        
            t.values.height = vsp.height();       
        } 

        return t.key;
    };

    static uint64_t stream2key(rs2_video_stream stream) {
        convert_t t;
        t.key = 0;

        t.values.def    = 0;
        t.values.type   = stream.type;  
        t.values.index  = stream.index; 
        t.values.uid    = stream.uid;    
        t.values.width  = stream.width;        
        t.values.height = stream.height;       
        t.values.fps    = stream.fps;          
        t.values.format = stream.fmt;

        return t.key;
    };

    static rs2_video_stream key2stream(uint64_t key) {
        convert_t t;
        t.key = key;

        rs2_video_stream stream;
        stream.type   = (rs2_stream)t.values.type;
        stream.index  = t.values.index;
        stream.uid    = t.values.uid;
        stream.width  = t.values.width;
        stream.height = t.values.height;
        stream.fps    = t.values.fps;
        stream.fmt    = (rs2_format)t.values.format;

        stream.bpp    = (stream.type == RS2_STREAM_INFRARED) ? 1 : 2;

        return stream;
    };

    static bool is_default(uint64_t key) {
        convert_t t;
        t.key = key;
        return t.values.def;
    };

private:
#pragma pack (push, 1)
    typedef union convert {
        uint64_t key;
        struct vals {
            uint8_t  def   : 2;
            uint8_t  type  : 4;
            uint8_t  index : 2;
            uint8_t  uid   ;
            uint16_t width ;
            uint16_t height;
            uint8_t  fps   ;
            uint8_t  format;
        } values;
    } convert_t;
#pragma pack(pop)
};
