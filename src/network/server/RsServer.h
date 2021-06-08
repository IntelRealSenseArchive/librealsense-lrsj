// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 Intel Corporation. All Rights Reserved.

#include <liveMedia.hh>
#include <GroupsockHelper.hh>
#include <BasicUsageEnvironment.hh>
#include <RTSPCommon.hh>

#include <librealsense2/rs.hpp>

#include <string>

class server
{
public:
    server(rs2::device dev, std::string addr, int port);
    ~server();

    void start();
    void stop();

private:
    UsageEnvironment* env;
    TaskScheduler* scheduler;

    rs2::device m_dev;

    RTSPServer* srv;

    std::thread m_httpd;
    void doHTTP();

    std::thread m_upgrade;
    void doUpgrade();
    std::string image;

    std::string m_sensors_desc; // sensors description
    std::stringstream m_extrinsics;   // streams extrinsics
};
