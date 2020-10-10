// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <vector>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/service/nvdrv/devices/nvdevice.h"

namespace Service::Nvidia::Devices {
class nvmap;

class nvhost_nvdec final : public nvdevice {
public:
    explicit nvhost_nvdec(Core::System& system, std::shared_ptr<nvmap> nvmap_dev);
    ~nvhost_nvdec() override;

    u32 ioctl(Ioctl command, const std::vector<u8>& input, const std::vector<u8>& input2,
              std::vector<u8>& output, std::vector<u8>& output2, IoctlCtrl& ctrl,
              IoctlVersion version) override;

private:
    class BufferMap final {
    public:
        constexpr BufferMap() = default;

        constexpr BufferMap(GPUVAddr start_addr, std::size_t size)
            : start_addr{start_addr}, end_addr{start_addr + size} {}

        constexpr BufferMap(GPUVAddr start_addr, std::size_t size, VAddr cpu_addr,
                            bool is_allocated)
            : start_addr{start_addr}, end_addr{start_addr + size}, cpu_addr{cpu_addr},
              is_allocated{is_allocated} {}

        constexpr VAddr StartAddr() const {
            return start_addr;
        }

        constexpr VAddr EndAddr() const {
            return end_addr;
        }

        constexpr std::size_t Size() const {
            return end_addr - start_addr;
        }

        constexpr VAddr CpuAddr() const {
            return cpu_addr;
        }

        constexpr bool IsAllocated() const {
            return is_allocated;
        }

    private:
        GPUVAddr start_addr{};
        GPUVAddr end_addr{};
        VAddr cpu_addr{};
        bool is_allocated{};
    };

    enum class IoctlCommand : u32_le {
        IocSetNVMAPfdCommand = 0x40044801,
        IocSubmit = 0xC0400001,
        IocGetSyncpoint = 0xC0080002,
        IocGetWaitbase = 0xC0080003,
        IocMapBuffer = 0xC01C0009,
        IocMapBuffer2 = 0xC16C0009,
        IocMapBuffer3 = 0xC15C0009,
        IocMapBufferEx = 0xC0A40009,
        IocUnmapBuffer = 0xC0A4000A,
        IocUnmapBuffer2 = 0xC16C000A,
        IocUnmapBuffer3 = 0xC01C000A,
        IocUnmapBuffer4 = 0xC15C000A,
        IocSetSubmitTimeout = 0x40040007,
    };

    struct IoctlSetNvmapFD {
        u32_le nvmap_fd;
    };
    static_assert(sizeof(IoctlSetNvmapFD) == 0x4, "IoctlSetNvmapFD is incorrect size");

    struct IoctlSubmit {
        u32_le cmd_buffer_count;
        u32_le relocation_count;
        u32_le syncpoint_count;
        u32_le fence_count;
    };
    static_assert(sizeof(IoctlSubmit) == 0x10, "IoctlSubmit has incorrect size");

    struct CommandBuffer {
        s32 memory_id;
        u32 offset;
        s32 word_count;
    };
    static_assert(sizeof(CommandBuffer) == 0xC, "CommandBuffer has incorrect size");

    struct Reloc {
        s32 cmdbuffer_memory;
        s32 cmdbuffer_offset;
        s32 target;
        s32 target_offset;
    };
    static_assert(sizeof(Reloc) == 0x10, "CommandBuffer has incorrect size");

    struct SyncptIncr {
        u32 id;
        u32 increments;
    };
    static_assert(sizeof(SyncptIncr) == 0x8, "CommandBuffer has incorrect size");

    struct Fence {
        u32 id;
        u32 value;
    };
    static_assert(sizeof(Fence) == 0x8, "CommandBuffer has incorrect size");

    struct IoctlGetSyncpoint {
        // Input
        u32_le param;
        // Output
        u32_le value;
    };
    static_assert(sizeof(IoctlGetSyncpoint) == 8, "IocGetIdParams has wrong size");

    struct IoctlGetWaitbase {
        u32 unknown; // seems to be ignored? Nintendo added this
        u32 value;
    };
    static_assert(sizeof(IoctlGetWaitbase) == 0x08, "IoctlGetWaitbase has incorrect size");

    // Used for mapping and unmapping command buffers
    struct MapBufferEntry {
        u32_le map_handle;
        u32_le map_address;
    };
    static_assert(sizeof(MapBufferEntry) == 0x8, "CommandBuffer has incorrect size");

    struct IoctlMapBuffer {
        u32_le num_entries;
        u32_le data_address; // Ignored by the driver.
        u32_le attach_host_ch_das;
    };
    static_assert(sizeof(IoctlMapBuffer) == 0x0C, "IoctlMapBuffer is incorrect size");

    struct IoctlUnmapBuffer {
        s64_le offset;
    };
    static_assert(sizeof(IoctlUnmapBuffer) == 8, "IoctlUnmapBuffer is incorrect size");

    // Used by stubs
    struct IoctlMapBufferEx {
        u32 unknown;
        u32 address_1;
        u32 address_2;
        INSERT_PADDING_BYTES(0x98);
    };
    static_assert(sizeof(IoctlMapBufferEx) == 0xA4, "IoctlMapBufferEx has incorrect size");

    // Used by stubs
    struct IoctlUnmapBufferEx {
        INSERT_PADDING_BYTES(0xA4);
    };
    static_assert(sizeof(IoctlUnmapBufferEx) == 0xA4, "IoctlUnmapBufferEx has incorrect size");

    // Used by stubs
    struct IoctlSubmitStub {
        INSERT_PADDING_BYTES(0x40);
    };
    static_assert(sizeof(IoctlSubmitStub) == 0x40, "IoctlSubmit has incorrect size");

    u32_le nvmap_fd{};
    u32_le submit_timeout{};

    u32 SetNVMAPfd(const std::vector<u8>& input, std::vector<u8>& output);
    u32 Submit(const std::vector<u8>& input, std::vector<u8>& output);
    u32 GetSyncpoint(const std::vector<u8>& input, std::vector<u8>& output);
    u32 GetWaitbase(const std::vector<u8>& input, std::vector<u8>& output);
    u32 MapBuffer(const std::vector<u8>& input, std::vector<u8>& output);
    u32 UnmapBuffer(const std::vector<u8>& input, std::vector<u8>& output);
    u32 MapBufferEx(const std::vector<u8>& input, std::vector<u8>& output);
    u32 UnmapBufferEx(const std::vector<u8>& input, std::vector<u8>& output);
    u32 SetSubmitTimeout(const std::vector<u8>& input, std::vector<u8>& output);

    // STUBBED fallback functions if user has nvdec disabled
    u32 SubmitStub(const std::vector<u8>& input, std::vector<u8>& output);
    u32 MapBufferStub(const std::vector<u8>& input, std::vector<u8>& output);

    std::optional<BufferMap> FindBufferMap(GPUVAddr gpu_addr) const;
    void AddBufferMap(GPUVAddr gpu_addr, std::size_t size, VAddr cpu_addr, bool is_allocated);
    std::optional<std::size_t> RemoveBufferMap(GPUVAddr gpu_addr);

    std::shared_ptr<nvmap> nvmap_dev;

    // This is expected to be ordered, therefore we must use a map, not unordered_map
    std::map<GPUVAddr, BufferMap> buffer_mappings;
};

} // namespace Service::Nvidia::Devices
