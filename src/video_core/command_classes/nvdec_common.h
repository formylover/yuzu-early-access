// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Tegra {
namespace NvdecCommon {

struct NvdecRegisters {
    INSERT_PADDING_WORDS(64);
    INSERT_PADDING_WORDS(64);
    u32 set_codec_id;
    INSERT_PADDING_WORDS(127);
    u32 set_platform_id;
    u32 picture_info_offset;
    u32 frame_bitstream_offset;
    u32 frame_number;
    u32 h264_slice_data_offsets;
    u32 h264_mv_dump_offset;
    INSERT_PADDING_WORDS(3);
    u32 frame_stats_offset;
    u32 h264_last_surface_luma_offset;
    u32 h264_last_surface_chroma_offset;
    std::array<u32, 17> surface_luma_offset;
    std::array<u32, 17> surface_chroma_offset;
    INSERT_PADDING_WORDS(66);
    u32 vp9_entropy_probs_offset;
    u32 vp9_backward_updates_offset;
    u32 vp9_last_frame_segmap_offset;
    u32 vp9_curr_frame_segmap_offset;
    u32 padding_0;
    u32 vp9_last_frame_mvs_offset;
    u32 vp9_curr_frame_mvs_offset;
    u32 padding_1;
};
static_assert(sizeof(NvdecRegisters) == (0x5E0), "NvdecRegisters is incorrect size");

enum class VideoCodec : u32 {
    None = 0x0,
    H264 = 0x3,
    Vp8 = 0x5,
    H265 = 0x7,
    Vp9 = 0x9,
};

} // namespace NvdecCommon
} // namespace Tegra
