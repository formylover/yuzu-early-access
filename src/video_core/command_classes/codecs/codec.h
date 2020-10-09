// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "video_core/command_classes/nvdec_common.h"

extern "C" {
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <libavcodec/avcodec.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

namespace Tegra {
class GPU;
struct VicRegisters;

namespace Decoder {
class H264;
class VP9;
} // namespace Decoder

class Codec {
public:
    explicit Codec(GPU& gpu);
    ~Codec();

    void SetTargetCodec(NvdecCommon::VideoCodec codec);

    void StateWrite(u32 offset, u32 arguments);

    void Decode();

    AVFrame* GetCurrentFrame();
    const AVFrame* GetCurrentFrame() const;
    NvdecCommon::VideoCodec GetCurrentCodec() const;
    const NvdecCommon::NvdecRegisters& GetNvdecState() const;

private:
    bool codec_swap{};
    bool initialized{};
    NvdecCommon::VideoCodec current_codec{NvdecCommon::VideoCodec::None};

    AVCodec* av_codec = nullptr;
    AVCodecContext* av_codec_ctx = nullptr;
    AVFrame* av_frame = nullptr;

    GPU& gpu;
    std::unique_ptr<Decoder::H264> h264_decoder;
    std::unique_ptr<Decoder::VP9> vp9_decoder;

    NvdecCommon::NvdecRegisters state{};
};

} // namespace Tegra
