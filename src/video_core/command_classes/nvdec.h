// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "video_core/command_classes/codecs/codec.h"

namespace Tegra {
class GPU;

class Nvdec {
public:
    enum class Method : u32 {
        SetVideoCodec = 0x80,
        Execute = 0xc0,
    };

    explicit Nvdec(GPU& gpu);
    ~Nvdec();

    void ProcessMethod(Nvdec::Method method, const std::vector<u32>& arguments);
    AVFrame* GetFrame();
    const AVFrame* GetFrame() const;

private:
    void Execute();

    GPU& gpu;
    std::unique_ptr<Tegra::Codec> codec;
};
} // namespace Tegra
