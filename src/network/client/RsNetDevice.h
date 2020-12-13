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

#include "JPEG2000DecodeFilter.h"
#include "JPEGDecodeFilter.h"
#include "LZ4DecodeFilter.h"
#include "LZ4VideoRTPSource.h"

#include <zstd.h>
#include <zstd_errors.h>

#include <lz4.h>

// forward
class rs_net_device; 
class RSRTSPClient;
class RsMediaSession;

class SafeQueue {
public:    
    SafeQueue() {};
    ~SafeQueue() {};

    void push(uint8_t* e) {
        std::lock_guard<std::mutex> lck (m);
        return q.push(e);
    };

    uint8_t* front() {
        std::lock_guard<std::mutex> lck (m);
        if (q.empty()) return NULL;
        return q.front();
    };

    void pop() {
        std::lock_guard<std::mutex> lck (m);
        if (q.empty()) return;
        return q.pop();
    };

    bool empty() {
        std::lock_guard<std::mutex> lck (m);
        return q.empty(); 
    };

private:
    std::queue<uint8_t*> q;
    std::mutex m;
};

using SoftSensor      = std::shared_ptr<rs2::software_sensor>;
using StreamProfile   = std::shared_ptr<rs2::video_stream_profile>;
using ProfileQPair    = std::pair<rs2::stream_profile, SafeQueue*>;
using ProfileQMap     = std::map<rs2::stream_profile, SafeQueue*>;

class RSSink : public MediaSink {
public:
    static RSSink* createNew(UsageEnvironment& env,
                                MediaSubsession& subsession, // identifies the kind of data that's being received
                                char const* streamId,        // identifies the stream itself
                                SafeQueue* q,
                                uint32_t threshold);         // number of stored bytes for the drop

private:
    RSSink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamIdm, SafeQueue* q, uint32_t threshold); // called only by "createNew()"
    virtual ~RSSink();

    static void afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned durationInMicroseconds); // callback
    void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned durationInMicroseconds); // member

private:
    static void getNextFrame(RSSink* sink);

    // redefined virtual functions:
    virtual Boolean continuePlaying();

private:
    uint32_t m_threshold;

    uint8_t* fReceiveBuffer;
    
    SafeQueue* m_frames;
    
    MediaSubsession& fSubsession;
    char* fStreamId;
};

// #define COMPRESSION_ENABLED
// #define COMPRESSION_ZSTD

#define CHUNK_SIZE (1024*2)
#pragma pack (push, 1)
typedef struct chunk_header{
    uint16_t size;
    uint32_t offset;
    uint8_t  status;
    uint8_t  meta_id;
    uint64_t meta_data;
} chunk_header_t;
#pragma pack(pop)
#define CHUNK_HLEN (sizeof(chunk_header_t))

class slib {
public:
    slib() {};
   ~slib() {};

    static uint64_t profile2key(rs2::video_stream_profile profile) {
        convert_t t;

        t.values.def    = 0 /* profile.is_default() */ ;
        t.values.type   = profile.stream_type();  
        t.values.index  = profile.stream_index(); 
        t.values.uid    = profile.unique_id();    
        t.values.width  = profile.width();        
        t.values.height = profile.height();       
        t.values.fps    = profile.fps();          
        t.values.format = profile.format();

        return t.key;
    };

    static uint64_t stream2key(rs2_video_stream stream) {
        convert_t t;

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
        return key >> 60;
    };

    static uint64_t strip_default(uint64_t key) {
        return key & 0x0FFFFFFFFFFFFFFF;
    };

private:
#pragma pack (push, 1)
    typedef union convert {
        uint64_t key;
        struct vals {
            uint8_t  def   : 4;
            uint8_t  type  : 4;
            uint8_t  index : 4;
            uint8_t  uid   : 4;
            uint16_t width ;
            uint16_t height;
            uint8_t  fps   ;
            uint8_t  format;
        } values;
    } convert_t;
#pragma pack(pop)

    static const uint64_t MASK_DEFAULT = 0xF000000000000000;
    static const uint64_t MASK_TYPE    = 0x0F00000000000000;
    static const uint64_t MASK_INDEX   = 0x00F0000000000000;
    static const uint64_t MASK_UID     = 0x000F000000000000;
    static const uint64_t MASK_WIDTH   = 0x0000FFFF00000000;
    static const uint64_t MASK_HEIGHT  = 0x00000000FFFF0000;
    static const uint64_t MASK_FPS     = 0x000000000000FF00;
    static const uint64_t MASK_FORMAT  = 0x00000000000000FF;    
};

class rs_net_stream {
public:
    rs_net_stream(rs2::stream_profile sp) : profile(sp) {
        rs2::video_stream_profile vsp = sp.as<rs2::video_stream_profile>();
        queue   = new SafeQueue;

        int bpp = vsp.stream_type() == RS2_STREAM_INFRARED ? 1 : 2;
        m_frame_raw = new uint8_t[vsp.width() * vsp.height() * bpp];
    };
    ~rs_net_stream() {
        if (m_frame_raw) delete [] m_frame_raw;
        if (queue) delete queue;
    };

    rs2_video_stream     stream;
    rs2::stream_profile  profile;
    SafeQueue*           queue;
    std::thread          thread;

    uint8_t*             m_frame_raw;
};

class rs_net_sensor {
public: 
    rs_net_sensor(rs2::software_device device, std::string name)
        : m_sw_device(device), m_name(name), m_streaming(false), m_dev_flag(false), m_eventLoopWatchVariable(0)
    {
        m_sw_sensor = std::make_shared<rs2::software_sensor>(m_sw_device.add_sensor(m_name));
    };

    ~rs_net_sensor() {};

    void set_mrl(std::string mrl) { m_mrl  = mrl; };
    void add_profile(uint64_t key) { 
        StreamProfile sp = std::make_shared<rs2::video_stream_profile>(m_sw_sensor->add_video_stream(slib::key2stream(key), slib::is_default(key)));

        // rs_net_stream* ns = new rs_net_stream;
        // ns->stream  = slib::key2stream(key);
        // ns->profile = std::make_shared<rs2::video_stream_profile>(m_sw_sensor->add_video_stream(ns->stream, slib::is_default(key)));
        // ns->queue   = new SafeQueue();

        // m_streams[slib::strip_default(key)] = ns;
    };

    bool is_streaming() { return m_sw_sensor->get_active_streams().size() > 0; };

    void start() {
        m_rtp = std::thread( [&](){ doRTP(); });
    };

    void doRTP();
    static void doControl(void* clientData) {
        rs_net_sensor* sensor = (rs_net_sensor*)clientData;
        sensor->doControl(); 
    };
    void doControl();
    void doDevice(uint64_t key);

private:    
    rs2::software_device m_sw_device;

    std::string    m_name;
    std::string    m_mrl;

    SoftSensor     m_sw_sensor;
    ProfileQMap    m_stream_q;

    std::map<uint64_t, rs_net_stream*> m_streams;

    std::thread    m_rtp;
    // std::thread    m_dev;
    volatile bool  m_dev_flag;

    RSRTSPClient*  m_rtspClient;
    char m_eventLoopWatchVariable;

    bool m_streaming;

    UsageEnvironment* m_env;
};
using NetSensor = std::shared_ptr<rs_net_sensor>;

class RSRTSPClient : public RTSPClient {
public:
    static RSRTSPClient* createNew(UsageEnvironment& env, char const* rtspURL) {
        return new RSRTSPClient(env, rtspURL);
    }

protected:
    RSRTSPClient(UsageEnvironment& env, char const* rtspURL)
        : RTSPClient(env, rtspURL, 0, "", 0, -1), m_pqm_it(NULL) {}

    virtual ~RSRTSPClient() {};

public:
    void playStreams(std::map<uint64_t, rs_net_stream*> streams);

    // void playStreams(ProfileQMap pqm);
    void shutdownStream();

    void prepareSession();
    void playSession();

    static void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString); // callback
    void continueAfterDESCRIBE(int resultCode, char* resultString); // member

    static void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString); // callback
    void continueAfterSETUP(int resultCode, char* resultString); // member

    static void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString); // callback
    void continueAfterPLAY(int resultCode, char* resultString); // member

    static void subsessionAfterPlaying(void* clientData);
    static void subsessionByeHandler(void* clientData, char const* reason);
    static void streamTimerHandler(void* clientData);

private:
    RsMediaSession*  m_session;
    MediaSubsession* m_subsession;

    std::string m_sdp;

    ProfileQMap m_pqm;
    ProfileQMap::iterator* m_pqm_it;

    std::map<uint64_t, rs_net_stream*> m_streams;
    std::map<uint64_t, rs_net_stream*>::iterator* m_streams_it;
};

class rs_net_device
{
public:
    rs_net_device(rs2::software_device sw_device, std::string ip_address);
   ~rs_net_device() {};

    rs2_intrinsics intrinsics;

private:
    std::string  m_ip_address;
    unsigned int m_ip_port;

    rs2::software_device m_device;

    std::vector<NetSensor> sensors;
};
