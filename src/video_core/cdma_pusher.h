// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <queue>

#include "common/bit_field.h"
#include "common/common_types.h"
#include "video_core/command_classes/sync_manager.h"

namespace Tegra {

class GPU;
class Nvdec;
class Vic;
class Host1x;

std::string HexDump(const std::vector<u8>& data);
std::string DumpArgs(GPU& gpu, const std::vector<u32>& arguments);

enum class ChSubmissionMode : u32 {
    SetClass = 0,
    Incrementing = 1,
    NonIncrementing = 2,
    Mask = 3,
    Immediate = 4,
    Restart = 5,
    Gather = 6,
};

enum class ChClassId : u32 {
    NoClass = 0x0,
    Host1x = 0x1,
    VideoEncodeMpeg = 0x20,
    VideoEncodeNvEnc = 0x21,
    VideoStreamingVi = 0x30,
    VideoStreamingIsp = 0x32,
    VideoStreamingIspB = 0x34,
    VideoStreamingViI2c = 0x36,
    GraphicsVic = 0x5d,
    Graphics3D = 0x60,
    GraphicsGpu = 0x61,
    Tsec = 0xe0,
    TsecB = 0xe1,
    NvJpg = 0xc0,
    NvDec = 0xf0
};

enum class ChMethod : u32 {
    Empty = 0,
    SetMethod = 0x10,
    SetData = 0x11,
};

union ChCommandHeader {
    u32 raw;
    BitField<0, 16, u32> value;
    BitField<16, 12, ChMethod> method_offset;
    BitField<28, 4, ChSubmissionMode> submission_mode;
};
static_assert(sizeof(ChCommandHeader) == sizeof(u32), "ChCommand header is an invalid size");

struct ChCommand {
    ChClassId class_id{};
    int method_offset{};
    std::vector<u32> arguments;
};

using ChCommandHeaderList = std::vector<Tegra::ChCommandHeader>;
using ChCommandList = std::vector<Tegra::ChCommand>;

struct ThiRegisters {
    u32_le IncrSyncpt;
    u32_le Reserved4;
    u32_le IncrSyncptErr;
    u32_le CtxswIncrSyncpt;
    INSERT_PADDING_WORDS(4);
    u32_le Ctxsw;
    u32_le Reserved24;
    u32_le ContSyncptEof;
    INSERT_PADDING_WORDS(5);
    u32_le Method0;
    u32_le Method1;
    INSERT_PADDING_WORDS(12);
    u32_le IntStatus;
    u32_le IntMask;
};

enum class ThiMethod : u32 {
    IncSyncpt = offsetof(ThiRegisters, IncrSyncpt) / 4,
    SetMethod0 = offsetof(ThiRegisters, Method0) / 4,
    SetMethod1 = offsetof(ThiRegisters, Method1) / 4,
};

class CDmaPusher {
public:
    explicit CDmaPusher(GPU& gpu);
    ~CDmaPusher();

    void Push(ChCommandHeaderList&& entries);

    void DispatchCalls();
    void Step();
    void ExecuteCommand(u32 offset, u32 data);

private:
    GPU& gpu;

    std::shared_ptr<Tegra::Nvdec> nvdec_processor;
    std::unique_ptr<Tegra::Vic> vic_processor;
    std::unique_ptr<Tegra::Host1x> host1x_processor;
    std::unique_ptr<SyncptIncrManager> nvdec_sync;
    std::unique_ptr<SyncptIncrManager> vic_sync;
    ChClassId current_class{};
    ThiRegisters vic_thi_state{};
    ThiRegisters nvdec_thi_state{};

    // Write arguments value to the ThiRegisters member at the specified offset
    void ThiStateWrite(ThiRegisters& state, u32 offset, const std::vector<u32>& arguments);

    // Format data as a hex string
    std::string HexDump(std::vector<u8>& data);

    // Format ChMethod arguments as string, used for debug logging
    std::string DumpArgs(GPU& gpu, const std::vector<u32>& arguments);

    int count{};
    int offset{};
    int mask{};
    bool incrementing{};

    // Queue of command lists to be processed
    std::queue<ChCommandHeaderList> cdma_queue;
};

} // namespace Tegra
