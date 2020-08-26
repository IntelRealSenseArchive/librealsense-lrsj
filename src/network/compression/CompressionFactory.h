// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 Intel Corporation. All Rights Reserved.

#pragma once

#include "ICompression.h"
#define IS_COMPRESSION_ENABLED 0 // enabled by default

typedef enum ZipMethod
{
    gzip,
    rvl,
    jpeg,
    lz,
} ZipMethod;

class CompressionFactory
{
public:
    static std::shared_ptr<ICompression> getObject(int t_width, int t_height, rs2_format t_format, rs2_stream t_streamType, int t_bpp);
    static bool isSupported(rs2_format t_format, rs2_stream t_streamType);
    
    static bool isEnabled();
    static void enable();
    static void disable();

private:
    static bool set_mode(bool mode, bool read);
};
