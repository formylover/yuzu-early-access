// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include <vector>
#include "common/cityhash.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "video_core/command_classes/codecs/vp9_types.h"
#include "video_core/command_classes/nvdec_common.h"

namespace Tegra {
class GPU;
enum class FrameType { KeyFrame = 0, InterFrame = 1 };
namespace Decoder {
namespace Util {

/// The Stream, VpxRangeEncoder, and VpxBitStreamWriter classes are used to compose the
/// header bitstreams.
class Stream {
public:
    Stream();
    ~Stream();

    void Seek(s32 cursor, s32 origin);
    u32 ReadByte();
    void WriteByte(u32 byte);
    std::size_t GetPosition() const {
        return position;
    }

    std::vector<u8>& GetBuffer() {
        return buffer;
    }

    const std::vector<u8>& GetBuffer() const {
        return buffer;
    }

private:
    std::vector<u8> buffer;
    std::size_t position{0};
};

} // namespace Util

class VpxRangeEncoder {
public:
    VpxRangeEncoder();
    ~VpxRangeEncoder();

    void WriteByte(u32 value);
    void Write(s32 value, s32 value_size);
    void Write(bool bit);
    void Write(bool bit, s32 probability);
    void End();

    std::vector<u8>& GetBuffer() {
        return base_stream.GetBuffer();
    }

    const std::vector<u8>& GetBuffer() const {
        return base_stream.GetBuffer();
    }

private:
    u8 PeekByte();
    Util::Stream base_stream{};
    u32 low_value{};
    u32 range{0xff};
    s32 count{-24};
    s32 half_probability{128};
    static constexpr std::array<s32, 256> norm_lut{
        0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
};

class VpxBitStreamWriter {
public:
    VpxBitStreamWriter();
    ~VpxBitStreamWriter();

    void WriteU(s32 value, s32 value_size);
    void WriteS(s32 value, s32 value_size);
    void WriteDeltaQ(s32 value);
    void WriteBit(bool state);
    void Flush();

    std::vector<u8>& GetByteArray();

private:
    void WriteBits(s32 value, s32 bit_count);
    s32 GetFreeBufferBits();

    s32 buffer_size{8};

    s32 buffer{};
    s32 buffer_pos{};
    std::vector<u8> byte_array;
};

class VP9 {
public:
    explicit VP9(GPU& gpu);
    ~VP9();

    std::vector<u8>& ComposeFrameHeader(NvdecCommon::NvdecRegisters& state);

    bool WasFrameHidden() const {
        return hidden;
    }

private:
    void PopulateProbability(GPUVAddr addr, std::size_t sz, std::vector<u8>& output);

    template <typename T, std::size_t N>
    void WriteProbabilityUpdate(VpxRangeEncoder& writer, const std::array<T, N>& new_prob,
                                const std::array<T, N>& old_prob);

    void WriteProbabilityUpdate(VpxRangeEncoder& writer, u8 New, u8 old);

    void WriteProbabilityDelta(VpxRangeEncoder& writer, u8 new_prob, u8 old_prob);
    s32 RemapProbability(s32 New, s32 old);
    s32 RecenterNonNeg(s32 New, s32 old);
    void EncodeTermSubExp(VpxRangeEncoder& writer, s32 value);
    bool WriteLessThan(VpxRangeEncoder& writer, s32 value, s32 test);

    void WriteCoefProbabilityUpdate(VpxRangeEncoder& writer, s32 tx_mode,
                                    const std::array<u8, 2304>& new_prob,
                                    const std::array<u8, 2304>& old_prob);

    template <typename T, std::size_t N>
    void WriteProbabilityUpdateAligned4(VpxRangeEncoder& writer, const std::array<T, N>& new_prob,
                                        const std::array<T, N>& old_prob);
    void WriteMvProbabilityUpdate(VpxRangeEncoder& writer, u8 new_prob, u8 old_prob);
    s32 CalcMinLog2TileCols(s32 frame_width);
    s32 CalcMaxLog2TileCols(s32 frame_width);

    Vp9PictureInfo GetVp9PictureInfo(const NvdecCommon::NvdecRegisters& state);
    void InsertEntropy(u32 offset, Vp9EntropyProbs& dst);
    Vp9FrameContainer GetCurrentFrame(const NvdecCommon::NvdecRegisters& state);

    std::vector<u8> ComposeCompressedHeader();
    VpxBitStreamWriter ComposeUncompressedHeader();

    GPU& gpu;
    std::vector<u8> frame;

    std::array<s8, 4> loop_filter_ref_deltas{};
    std::array<s8, 2> loop_filter_mode_deltas{};

    bool hidden;
    s64 current_frame_number = -2; // since we buffer 2 frames
    s32 grace_period = 6;          // frame offsets need to stabilize
    std::array<RefPoolElement, 8> reference_pool{};
    std::array<FrameContexts, 4> frame_ctxs{};
    Vp9FrameContainer next_frame{};
    Vp9FrameContainer next_next_frame{};
    bool swap_next_golden{};

    Vp9PictureInfo current_frame_info{};
    Vp9EntropyProbs prev_frame_probs{};

    s32 diff_update_probability = 252;
    s32 frame_sync_code = 0x498342;
    static constexpr std::array<s32, 254> map_lut = {
        20,  21,  22,  23,  24,  25,  0,   26,  27,  28,  29,  30,  31,  32,  33,  34,  35,
        36,  37,  1,   38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  2,   50,
        51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  3,   62,  63,  64,  65,  66,
        67,  68,  69,  70,  71,  72,  73,  4,   74,  75,  76,  77,  78,  79,  80,  81,  82,
        83,  84,  85,  5,   86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  6,
        98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 7,   110, 111, 112, 113,
        114, 115, 116, 117, 118, 119, 120, 121, 8,   122, 123, 124, 125, 126, 127, 128, 129,
        130, 131, 132, 133, 9,   134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145,
        10,  146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 11,  158, 159, 160,
        161, 162, 163, 164, 165, 166, 167, 168, 169, 12,  170, 171, 172, 173, 174, 175, 176,
        177, 178, 179, 180, 181, 13,  182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192,
        193, 14,  194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 15,  206, 207,
        208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 16,  218, 219, 220, 221, 222, 223,
        224, 225, 226, 227, 228, 229, 17,  230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
        240, 241, 18,  242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 19,
    };
};

} // namespace Decoder
} // namespace Tegra
