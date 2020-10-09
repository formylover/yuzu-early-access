// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include "common/assert.h"
#include "nvdec.h"
#include "vic.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"
#include "video_core/texture_cache/surface_params.h"

extern "C" {
#include <libswscale/swscale.h>
}

namespace Tegra {

Vic::Vic(GPU& gpu, std::shared_ptr<Nvdec> nvdec_processor)
    : gpu(gpu), nvdec_processor(std::move(nvdec_processor)) {}
Vic::~Vic() = default;

void Vic::VicDeviceWrite(u32 offset, u32 arguments) {
    LOG_DEBUG(Service_NVDRV, "GOT OFFSET 0x{:X} WITH DATA 0x{:X}", offset * 4, arguments);
    u8* state_offset = reinterpret_cast<u8*>(&vic_state) + offset * sizeof(u32);
    std::memcpy(state_offset, &arguments, sizeof(u32));
}

void Vic::ProcessMethod(Vic::Method method, const std::vector<u32>& arguments) {
    LOG_DEBUG(HW_GPU, "Vic method 0x{:X}", static_cast<u32>(method));
    VicDeviceWrite(static_cast<u32>(method), arguments[0]);

    switch (method) {
    case Method::Execute:
        Execute();
        break;
    case Method::SetConfigStructOffset:
        config_struct_address = gpu.MemoryManager().GpuAddressFromPinned(arguments[0]);
        break;
    case Method::SetOutputSurfaceLumaOffset:
        output_surface_luma_address = gpu.MemoryManager().GpuAddressFromPinned(arguments[0]);
        break;
    case Method::SetOutputSurfaceChromaUOffset:
        output_surface_chroma_u_address = gpu.MemoryManager().GpuAddressFromPinned(arguments[0]);
        break;
    case Method::SetOutputSurfaceChromaVOffset:
        output_surface_chroma_v_address = gpu.MemoryManager().GpuAddressFromPinned(arguments[0]);
        break;
    default:
        LOG_DEBUG(Service_NVDRV, "NV-Vic unimplemented method {}", static_cast<u32>(method));
    }
}

void Vic::Execute() {
    if (output_surface_luma_address == 0) {
        // TODO(ameerj): Invetigate Link's Awakening bug causing this.
        LOG_CRITICAL(Service_NVDRV, "VIC LUMA ADDRESS NOT SET. RECEIVED PINNED 0x{:X}",
                     vic_state.output_surface.luma_offset);
        // This seems to be the fallback actual address to write to.
        output_surface_luma_address = gpu.MemoryManager().GpuAddressFromPinned(
            vic_state.surfacex_slots[0][0].luma_offset - 1);
        return;
    }

    const VicConfig config{gpu.MemoryManager().Read<u64>(config_struct_address + 0x20)};

    switch (static_cast<VideoPixelFormat>(config.pixel_format.Value())) {
    case VideoPixelFormat::Rgba8: {
        LOG_DEBUG(Service_NVDRV, "Writing RGB Frame");
        const auto frame = nvdec_processor->GetFrame();

        if (!frame || frame->width == 0 || frame->height == 0) {
            return;
        }

        if (scaler_ctx == nullptr || frame->width != scaler_width ||
            frame->height != scaler_height) {
            sws_freeContext(scaler_ctx);
            scaler_ctx = nullptr;
            // FFmpeg returns all frames in YUV420, convert it into RGBA format
            scaler_ctx =
                sws_getContext(frame->width, frame->height, AV_PIX_FMT_YUV420P, frame->width,
                               frame->height, AV_PIX_FMT_RGBA, 0, nullptr, nullptr, nullptr);

            scaler_width = frame->width;
            scaler_height = frame->height;
        }

        // Get RGB frame
        const std::size_t linear_size = frame->width * frame->height * 4;
        u8* rgb_frame_buffer = static_cast<u8*>(av_malloc(linear_size));
        const std::array<int, 1> rgb_stride{frame->width * 4};
        const std::array<u8*, 1> rgb_frame_buf_addr{rgb_frame_buffer};

        sws_scale(scaler_ctx, frame->data, frame->linesize, 0, frame->height,
                  rgb_frame_buf_addr.data(), rgb_stride.data());

        const u32 blk_kind = static_cast<u32>(config.block_linear_kind);
        if (blk_kind != 0) {
            // swizzle pitch linear to block linear
            const u32 block_height = static_cast<u32>(config.block_linear_height_log2);
            const auto size = Tegra::Texture::CalculateSize(true, 4, frame->width, frame->height, 1,
                                                            block_height, 0);
            std::vector<u8> swizzled_data(size);
            Tegra::Texture::CopySwizzledData(frame->width, frame->height, 1, 4, 4,
                                             swizzled_data.data(), rgb_frame_buffer, false,
                                             block_height, 0, 1);

            gpu.Maxwell3D().OnMemoryWrite();
            gpu.MemoryManager().WriteBlock(output_surface_luma_address, swizzled_data.data(), size);
        } else {
            // send pitch linear frame
            gpu.Maxwell3D().OnMemoryWrite();
            gpu.MemoryManager().WriteBlock(output_surface_luma_address, rgb_frame_buf_addr.data(),
                                           linear_size);
        }

        av_free(rgb_frame_buffer);
        break;
    }
    case VideoPixelFormat::Yuv420: {
        LOG_DEBUG(Service_NVDRV, "Writing YUV420 Frame");

        const auto frame = nvdec_processor->GetFrame();

        if (!frame || frame->width == 0 || frame->height == 0) {
            return;
        }

        const std::size_t surface_width = config.surface_width_minus1 + 1;
        const std::size_t surface_height = config.surface_height_minus1 + 1;
        const std::size_t src_half_width = frame->width / 2;
        const std::size_t half_width = surface_width / 2;
        const std::size_t half_height = config.surface_height_minus1 / 2;
        const std::size_t aligned_width = (surface_width + 0xff) & ~0xff;

        const auto luma_ptr = frame->data[0];
        const auto chroma_b_ptr = frame->data[1];
        const auto chroma_r_ptr = frame->data[2];
        const auto stride = frame->linesize[0];
        const auto half_stride = frame->linesize[1];

        LOG_DEBUG(Service_NVDRV,
                  "Writing YUV420 Frame {}x{} to locations: 0x{:X} 0x{:X} from ptrs: 0x{:X} "
                  "0x{:X} 0x{:X}",
                  frame->width, frame->height, output_surface_luma_address,
                  output_surface_chroma_u_address, u64(luma_ptr), u64(chroma_b_ptr),
                  u64(chroma_r_ptr));

        gpu.Maxwell3D().OnMemoryWrite();

        std::vector<u8> luma_buffer(aligned_width * surface_height);
        std::vector<u8> chroma_buffer(aligned_width * half_height);

        // Populate luma buffer
        for (std::size_t y = 0; y < surface_height - 1; y++) {
            std::size_t src = y * stride;
            std::size_t dst = y * aligned_width;

            std::size_t size = surface_width;

            for (std::size_t offset = 0; offset < size; offset++) {
                luma_buffer[dst + offset] = *(luma_ptr + src + offset);
            }
        }
        gpu.MemoryManager().WriteBlock(output_surface_luma_address, luma_buffer.data(),
                                       luma_buffer.size());

        // Populate chroma buffer from both channels with interleaving.
        for (std::size_t y = 0; y < half_height; y++) {
            std::size_t src = y * half_stride;
            std::size_t dst = y * aligned_width;

            for (std::size_t x = 0; x < half_width; x++) {
                chroma_buffer[dst + x * 2] = *(chroma_b_ptr + src + x);
                chroma_buffer[dst + x * 2 + 1] = *(chroma_r_ptr + src + x);
            }
        }
        gpu.MemoryManager().WriteBlock(output_surface_chroma_u_address, chroma_buffer.data(),
                                       chroma_buffer.size());
        break;
    }
    default:
        UNIMPLEMENTED_MSG("Unknown video pixel format {}", config.pixel_format.Value());
    }
}

} // namespace Tegra
