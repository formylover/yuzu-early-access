// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <numeric>
#include "common/assert.h"
#include "video_core/command_classes/codecs/vp9.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"

namespace Tegra::Decoder {

VP9::VP9(GPU& gpu) : gpu(gpu) {
    reference_pool[0].ref = Ref::LAST;
    reference_pool[1].ref = Ref::GOLDEN;
    reference_pool[2].ref = Ref::ALTREF;
}

VP9::~VP9() = default;

void VP9::PopulateProbability(GPUVAddr addr, std::size_t sz, std::vector<u8>& output) {
    output.resize(sz);
    gpu.MemoryManager().ReadBlock(addr, output.data(), sz);
}

void VP9::WriteProbabilityUpdate(VpxRangeEncoder& writer, u8 new_prob, u8 old_prob) {
    const bool update = new_prob != old_prob;

    writer.Write(update, diff_update_probability);

    if (update) {
        WriteProbabilityDelta(writer, new_prob, old_prob);
    }
}
template <typename T, std::size_t N>
void VP9::WriteProbabilityUpdate(VpxRangeEncoder& writer, const std::array<T, N>& new_prob,
                                 const std::array<T, N>& old_prob) {
    for (std::size_t offset = 0; offset < new_prob.size(); ++offset) {
        WriteProbabilityUpdate(writer, new_prob[offset], old_prob[offset]);
    }
}

template <typename T, std::size_t N>
void VP9::WriteProbabilityUpdateAligned4(VpxRangeEncoder& writer, const std::array<T, N>& new_prob,
                                         const std::array<T, N>& old_prob) {
    for (std::size_t offset = 0; offset < new_prob.size(); offset += 4) {
        WriteProbabilityUpdate(writer, new_prob[offset + 0], old_prob[offset + 0]);
        WriteProbabilityUpdate(writer, new_prob[offset + 1], old_prob[offset + 1]);
        WriteProbabilityUpdate(writer, new_prob[offset + 2], old_prob[offset + 2]);
    }
}

void VP9::WriteProbabilityDelta(VpxRangeEncoder& writer, u8 new_prob, u8 old_prob) {
    const int delta = RemapProbability(new_prob, old_prob);

    EncodeTermSubExp(writer, delta);
}

s32 VP9::RemapProbability(s32 new_prob, s32 old_prob) {
    new_prob--;
    old_prob--;

    int index;

    if (old_prob * 2 <= 0xff) {
        index = RecenterNonNeg(new_prob, old_prob) - 1;
    } else {
        index = RecenterNonNeg(0xff - 1 - new_prob, 0xff - 1 - old_prob) - 1;
    }

    return map_lut[index];
}

s32 VP9::RecenterNonNeg(s32 new_prob, s32 old_prob) {
    if (new_prob > old_prob * 2) {
        return new_prob;
    } else if (new_prob >= old_prob) {
        return (new_prob - old_prob) * 2;
    } else {
        return (old_prob - new_prob) * 2 - 1;
    }
}

void VP9::EncodeTermSubExp(VpxRangeEncoder& writer, s32 value) {
    if (WriteLessThan(writer, value, 16)) {
        writer.Write(value, 4);
    } else if (WriteLessThan(writer, value, 32)) {
        writer.Write(value - 16, 4);
    } else if (WriteLessThan(writer, value, 64)) {
        writer.Write(value - 32, 5);
    } else {
        value -= 64;

        constexpr s32 size = 8;

        const s32 mask = (1 << size) - 191;

        const s32 delta = value - mask;

        if (delta < 0) {
            writer.Write(value, size - 1);
        } else {
            writer.Write(delta / 2 + mask, size - 1);
            writer.Write(delta & 1, 1);
        }
    }
}

bool VP9::WriteLessThan(VpxRangeEncoder& writer, s32 value, s32 test) {
    const bool is_lt = value < test;
    writer.Write(!is_lt);
    return is_lt;
}

void VP9::WriteCoefProbabilityUpdate(VpxRangeEncoder& writer, s32 tx_mode,
                                     const std::array<u8, 2304>& new_prob,
                                     const std::array<u8, 2304>& old_prob) {
    // Note: There's 1 byte added on each packet for alignment,
    // this byte is ignored when doing updates.
    const s32 block_bytes = 2 * 2 * 6 * 6 * 4;

    const auto needs_update = [&](s32 base_index) -> bool {
        s32 index = base_index;
        for (s32 i = 0; i < 2; i++) {
            for (s32 j = 0; j < 2; j++) {
                for (s32 k = 0; k < 6; k++) {
                    for (s32 l = 0; l < 6; l++) {
                        if (new_prob[index + 0] != old_prob[index + 0] ||
                            new_prob[index + 1] != old_prob[index + 1] ||
                            new_prob[index + 2] != old_prob[index + 2]) {
                            return true;
                        }

                        index += 4;
                    }
                }
            }
        }
        return false;
    };

    for (s32 block_index = 0; block_index < 4; block_index++) {
        const s32 base_index = block_index * block_bytes;
        const bool update = needs_update(base_index);
        writer.Write(update);

        if (update) {
            s32 index = base_index;
            for (s32 i = 0; i < 2; i++) {
                for (s32 j = 0; j < 2; j++) {
                    for (s32 k = 0; k < 6; k++) {
                        for (s32 l = 0; l < 6; l++) {
                            if (k != 0 || l < 3) {
                                WriteProbabilityUpdate(writer, new_prob[index + 0],
                                                       old_prob[index + 0]);
                                WriteProbabilityUpdate(writer, new_prob[index + 1],
                                                       old_prob[index + 1]);
                                WriteProbabilityUpdate(writer, new_prob[index + 2],
                                                       old_prob[index + 2]);
                            }
                            index += 4;
                        }
                    }
                }
            }
        }

        if (block_index == tx_mode) {
            break;
        }
    }
}

void VP9::WriteMvProbabilityUpdate(VpxRangeEncoder& writer, u8 new_prob, u8 old_prob) {
    const bool update = new_prob != old_prob;
    writer.Write(update, diff_update_probability);

    if (update) {
        writer.Write(new_prob >> 1, 7);
    }
}

s32 VP9::CalcMinLog2TileCols(s32 frame_width) {
    const s32 sb64_cols = (frame_width + 63) / 64;
    s32 min_log2 = 0;

    while ((64 << min_log2) < sb64_cols) {
        min_log2++;
    }

    return min_log2;
}

s32 VP9::CalcMaxLog2TileCols(s32 frameWidth) {
    const s32 sb64_cols = (frameWidth + 63) / 64;
    s32 max_log2 = 1;

    while ((sb64_cols >> max_log2) >= 4) {
        max_log2++;
    }

    return max_log2 - 1;
}

Vp9PictureInfo VP9::GetVp9PictureInfo(const NvdecCommon::NvdecRegisters& state) {
    PictureInfo picture_info{};
    gpu.MemoryManager().ReadBlock(state.picture_info_offset, &picture_info, sizeof(PictureInfo));
    Vp9PictureInfo vp9_info = picture_info.Convert();

    InsertEntropy(state.vp9_entropy_probs_offset, vp9_info.entropy);
    std::memcpy(vp9_info.frame_offsets.data(), state.surface_luma_offset.data(), 4 * sizeof(u32));
    return std::move(vp9_info);
}

void VP9::InsertEntropy(u32 offset, Vp9EntropyProbs& dst) {
    EntropyProbs entropy{};
    gpu.MemoryManager().ReadBlock(offset, &entropy, sizeof(EntropyProbs));
    entropy.Convert(dst);
}

Vp9FrameContainer VP9::GetCurrentFrame(const NvdecCommon::NvdecRegisters& state) {
    Vp9FrameContainer frame{};
    {
        gpu.SyncGuestHost();
        frame.info = std::move(GetVp9PictureInfo(state));

        frame.bit_stream.resize(frame.info.bitstream_size);
        gpu.MemoryManager().ReadBlock(state.frame_bitstream_offset, frame.bit_stream.data(),
                                      frame.info.bitstream_size);
    }
    // Buffer two frames, saving the last show frame info
    if (next_next_frame.bit_stream.size() != 0) {
        Vp9FrameContainer temp{};
        temp.info = frame.info;
        temp.bit_stream = frame.bit_stream;
        next_next_frame.info.show_frame = frame.info.last_frame_shown;
        frame.info = next_next_frame.info;
        frame.bit_stream = next_next_frame.bit_stream;
        next_next_frame = std::move(temp);

        if (next_frame.bit_stream.size() != 0) {
            Vp9FrameContainer temp{};
            temp.info = frame.info;
            temp.bit_stream = frame.bit_stream;
            next_frame.info.show_frame = frame.info.last_frame_shown;
            frame.info = next_frame.info;
            frame.bit_stream = next_frame.bit_stream;
            next_frame = std::move(temp);
        } else {
            next_frame.info = frame.info;
            next_frame.bit_stream = frame.bit_stream;
        }
    } else {
        next_next_frame.info = frame.info;
        next_next_frame.bit_stream = frame.bit_stream;
    }
    return frame;
}

std::vector<u8> VP9::ComposeCompressedHeader() {
    VpxRangeEncoder writer{};

    if (!current_frame_info.lossless) {
        if (static_cast<u32>(current_frame_info.transform_mode) >= 3) {
            writer.Write(3, 2);
            writer.Write(current_frame_info.transform_mode == 4);
        } else {
            writer.Write(current_frame_info.transform_mode, 2);
        }
    }

    if (current_frame_info.transform_mode == 4) {
        // tx_mode_probs() in the spec
        WriteProbabilityUpdate(writer, current_frame_info.entropy.tx_8x8_prob,
                               prev_frame_probs.tx_8x8_prob);
        WriteProbabilityUpdate(writer, current_frame_info.entropy.tx_16x16_prob,
                               prev_frame_probs.tx_16x16_prob);
        WriteProbabilityUpdate(writer, current_frame_info.entropy.tx_32x32_prob,
                               prev_frame_probs.tx_32x32_prob);
        if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
            prev_frame_probs.tx_8x8_prob = current_frame_info.entropy.tx_8x8_prob;
            prev_frame_probs.tx_16x16_prob = current_frame_info.entropy.tx_16x16_prob;
            prev_frame_probs.tx_32x32_prob = current_frame_info.entropy.tx_32x32_prob;
        }
    }
    // read_coef_probs()  in the spec
    WriteCoefProbabilityUpdate(writer, current_frame_info.transform_mode,
                               current_frame_info.entropy.coef_probs, prev_frame_probs.coef_probs);
    // read_skip_probs()  in the spec
    WriteProbabilityUpdate(writer, current_frame_info.entropy.skip_probs,
                           prev_frame_probs.skip_probs);

    if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
        prev_frame_probs.coef_probs = current_frame_info.entropy.coef_probs;
        prev_frame_probs.skip_probs = current_frame_info.entropy.skip_probs;
    }

    if (!current_frame_info.intra_only) {
        // read_inter_probs() in the spec
        WriteProbabilityUpdateAligned4(writer, current_frame_info.entropy.inter_mode_prob,
                                       prev_frame_probs.inter_mode_prob);
        if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
            prev_frame_probs.inter_mode_prob = current_frame_info.entropy.inter_mode_prob;
        }

        if (current_frame_info.interp_filter == 4) {
            // read_interp_filter_probs() in the spec
            WriteProbabilityUpdate(writer, current_frame_info.entropy.switchable_interp_prob,
                                   prev_frame_probs.switchable_interp_prob);
            if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
                prev_frame_probs.switchable_interp_prob =
                    current_frame_info.entropy.switchable_interp_prob;
            }
        }

        // read_is_inter_probs() in the spec
        WriteProbabilityUpdate(writer, current_frame_info.entropy.intra_inter_prob,
                               prev_frame_probs.intra_inter_prob);
        if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
            prev_frame_probs.intra_inter_prob = current_frame_info.entropy.intra_inter_prob;
        }
        // frame_reference_mode() in the spec
        if ((current_frame_info.ref_frame_sign_bias[1] & 1) !=
                (current_frame_info.ref_frame_sign_bias[2] & 1) ||
            (current_frame_info.ref_frame_sign_bias[1] & 1) !=
                (current_frame_info.ref_frame_sign_bias[3] & 1)) {
            if (current_frame_info.reference_mode >= 1) {
                writer.Write(1, 1);
                writer.Write(current_frame_info.reference_mode == 2);
            } else {
                writer.Write(0, 1);
            }
        }

        // frame_reference_mode_probs() in the spec
        if (current_frame_info.reference_mode == 2) {
            WriteProbabilityUpdate(writer, current_frame_info.entropy.comp_inter_prob,
                                   prev_frame_probs.comp_inter_prob);
            if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
                prev_frame_probs.comp_inter_prob = current_frame_info.entropy.comp_inter_prob;
            }
        }

        if (current_frame_info.reference_mode != 1) {
            WriteProbabilityUpdate(writer, current_frame_info.entropy.single_ref_prob,
                                   prev_frame_probs.single_ref_prob);
            if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
                prev_frame_probs.single_ref_prob = current_frame_info.entropy.single_ref_prob;
            }
        }

        if (current_frame_info.reference_mode != 0) {
            WriteProbabilityUpdate(writer, current_frame_info.entropy.comp_ref_prob,
                                   prev_frame_probs.comp_ref_prob);
            if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
                prev_frame_probs.comp_ref_prob = current_frame_info.entropy.comp_ref_prob;
            }
        }

        // read_y_mode_probs
        for (std::size_t index = 0; index < current_frame_info.entropy.y_mode_prob.size();
             ++index) {
            WriteProbabilityUpdate(writer, current_frame_info.entropy.y_mode_prob[index],
                                   prev_frame_probs.y_mode_prob[index]);
        }
        if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
            prev_frame_probs.y_mode_prob = current_frame_info.entropy.y_mode_prob;
        }
        // read_partition_probs
        WriteProbabilityUpdateAligned4(writer, current_frame_info.entropy.partition_prob,
                                       prev_frame_probs.partition_prob);
        if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
            prev_frame_probs.partition_prob = current_frame_info.entropy.partition_prob;
        }

        // mv_probs
        for (int i = 0; i < 3; i++) {
            WriteMvProbabilityUpdate(writer, current_frame_info.entropy.joints[i],
                                     prev_frame_probs.joints[i]);
        }
        if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
            prev_frame_probs.joints = current_frame_info.entropy.joints;
        }

        for (int i = 0; i < 2; i++) {
            WriteMvProbabilityUpdate(writer, current_frame_info.entropy.sign[i],
                                     prev_frame_probs.sign[i]);

            for (int j = 0; j < 10; j++) {
                const int index = i * 10 + j;

                WriteMvProbabilityUpdate(writer, current_frame_info.entropy.classes[index],
                                         prev_frame_probs.classes[index]);
            }

            WriteMvProbabilityUpdate(writer, current_frame_info.entropy.class_0[i],
                                     prev_frame_probs.class_0[i]);

            for (int j = 0; j < 10; j++) {
                const int index = i * 10 + j;

                WriteMvProbabilityUpdate(writer, current_frame_info.entropy.prob_bits[index],
                                         prev_frame_probs.prob_bits[index]);
            }
        }

        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 2; j++) {
                for (int k = 0; k < 3; k++) {
                    const int index = i * 2 * 3 + j * 3 + k;

                    WriteMvProbabilityUpdate(writer, current_frame_info.entropy.class_0_fr[index],
                                             prev_frame_probs.class_0_fr[index]);
                }
            }

            for (int j = 0; j < 3; j++) {
                const int index = i * 3 + j;

                WriteMvProbabilityUpdate(writer, current_frame_info.entropy.fr[index],
                                         prev_frame_probs.fr[index]);
            }
        }

        if (current_frame_info.allow_high_precision_mv) {
            for (int index = 0; index < 2; index++) {
                WriteMvProbabilityUpdate(writer, current_frame_info.entropy.class_0_hp[index],
                                         prev_frame_probs.class_0_hp[index]);
                WriteMvProbabilityUpdate(writer, current_frame_info.entropy.high_precision[index],
                                         prev_frame_probs.high_precision[index]);
            }
        }

        // save previous probs
        if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
            prev_frame_probs.sign = current_frame_info.entropy.sign;
            prev_frame_probs.classes = current_frame_info.entropy.classes;
            prev_frame_probs.class_0 = current_frame_info.entropy.class_0;
            prev_frame_probs.prob_bits = current_frame_info.entropy.prob_bits;
            prev_frame_probs.class_0_fr = current_frame_info.entropy.class_0_fr;
            prev_frame_probs.fr = current_frame_info.entropy.fr;
            prev_frame_probs.class_0_hp = current_frame_info.entropy.class_0_hp;
            prev_frame_probs.high_precision = current_frame_info.entropy.high_precision;
        }
    }

    writer.End();
    return writer.GetBuffer();

    const auto writer_bytearray = writer.GetBuffer();

    std::vector<u8> compressed_header(writer_bytearray.size());
    std::memcpy(compressed_header.data(), writer_bytearray.data(), writer_bytearray.size());
    return compressed_header;
}

VpxBitStreamWriter VP9::ComposeUncompressedHeader() {
    VpxBitStreamWriter uncomp_writer{};

    uncomp_writer.WriteU(2, 2);                                      // Frame marker.
    uncomp_writer.WriteU(0, 2);                                      // Profile.
    uncomp_writer.WriteBit(false);                                   // Show existing frame.
    uncomp_writer.WriteBit(!current_frame_info.is_key_frame);        // is key frame?
    uncomp_writer.WriteBit(current_frame_info.show_frame);           // show frame?
    uncomp_writer.WriteBit(current_frame_info.error_resilient_mode); // error reslience

    if (current_frame_info.is_key_frame) {
        uncomp_writer.WriteU(frame_sync_code, 24);
        uncomp_writer.WriteU(0, 3); // Color space.
        uncomp_writer.WriteU(0, 1); // Color range.
        uncomp_writer.WriteU(current_frame_info.frame_size.width - 1, 16);
        uncomp_writer.WriteU(current_frame_info.frame_size.height - 1, 16);
        uncomp_writer.WriteBit(false); // Render and frame size different.

        // Reset context
        prev_frame_probs = default_probs;
        swap_next_golden = false;
        loop_filter_ref_deltas.fill(0);
        loop_filter_mode_deltas.fill(0);

        // allow frames offsets to stabilize before checking for golden frames
        grace_period = 4;

        // On key frames, all frame slots are set to the current frame,
        // so the value of the selected slot doesn't really matter.
        reference_pool[0].frame = current_frame_number;
        reference_pool[1].frame = current_frame_number;
        reference_pool[2].frame = current_frame_number;

        frame_ctxs.fill({current_frame_number, false, default_probs});

        // intra only, meaning the frame can be recreated with no other references
        current_frame_info.intra_only = true;

    } else {
        std::array<s32, 3> ref_frame_index;

        if (!current_frame_info.show_frame) {
            uncomp_writer.WriteBit(current_frame_info.intra_only);
            swap_next_golden = !swap_next_golden;
        } else {
            current_frame_info.intra_only = false;
        }
        if (!current_frame_info.error_resilient_mode) {
            uncomp_writer.WriteU(0, 2); // Reset frame context.
        }

        // Last, Golden, Altref frames
        ref_frame_index = std::array<s32, 3>{0, 1, 2};

        // set when next frame is hidden
        // altref and golden references are swapped
        if (swap_next_golden) {
            ref_frame_index = std::array<s32, 3>{0, 2, 1};
        }

        // update Last Frame
        u64 refresh_frame_flags = 1;

        // golden frame may refresh, determined if the next golden frame offset is changed
        bool golden_refresh = false;
        if (grace_period < 0) {
            for (int index = 1; index < 3; ++index) {
                if (current_frame_info.frame_offsets[index] !=
                    next_frame.info.frame_offsets[index]) {
                    current_frame_info.refresh_frame[index] = true;
                    golden_refresh = true;
                    grace_period = 3;
                }
            }
        }

        if (current_frame_info.show_frame &&
            (!next_frame.info.show_frame || next_frame.info.is_key_frame)) {
            // Update golden (not x86)
            refresh_frame_flags = swap_next_golden ? 2 : 4;
        }

        if (!current_frame_info.show_frame) {
            // Update altref
            refresh_frame_flags = swap_next_golden ? 2 : 4;
        } else if (golden_refresh) {
            refresh_frame_flags = 3;
        }

        if (current_frame_info.intra_only) {
            uncomp_writer.WriteU(frame_sync_code, 24);
            uncomp_writer.WriteU(static_cast<s32>(refresh_frame_flags), 8);
            uncomp_writer.WriteU(current_frame_info.frame_size.width - 1, 16);
            uncomp_writer.WriteU(current_frame_info.frame_size.height - 1, 16);
            uncomp_writer.WriteBit(false); // Render and frame size different.

        } else {
            uncomp_writer.WriteU(static_cast<s32>(refresh_frame_flags), 8);

            for (int index = 1; index < 4; index++) {
                uncomp_writer.WriteU(ref_frame_index[index - 1], 3);
                uncomp_writer.WriteU(current_frame_info.ref_frame_sign_bias[index], 1);
            }

            uncomp_writer.WriteBit(true);  // Frame size with refs.
            uncomp_writer.WriteBit(false); // Render and frame size different.
            uncomp_writer.WriteBit(current_frame_info.allow_high_precision_mv);
            uncomp_writer.WriteBit(current_frame_info.interp_filter == 4);

            if (current_frame_info.interp_filter != 4) {
                uncomp_writer.WriteU(current_frame_info.interp_filter, 2);
            }
        }
    }

    if (!current_frame_info.error_resilient_mode) {
        uncomp_writer.WriteBit(true); // Refresh frame context. where do i get this info from?
        uncomp_writer.WriteBit(true); // Frame parallel decoding mode.
    }

    int frame_ctx_idx = 0;
    if (!current_frame_info.show_frame) {
        frame_ctx_idx = 1;
    }

    uncomp_writer.WriteU(frame_ctx_idx, 2); // Frame context index.
    prev_frame_probs =
        frame_ctxs[frame_ctx_idx].probs; // reference probabilities for compressed header
    frame_ctxs[frame_ctx_idx] = {current_frame_number, false, current_frame_info.entropy};

    uncomp_writer.WriteU(current_frame_info.first_level, 6);
    uncomp_writer.WriteU(current_frame_info.sharpness_level, 3);
    uncomp_writer.WriteBit(current_frame_info.mode_ref_delta_enabled);

    if (current_frame_info.mode_ref_delta_enabled) {
        // check if ref deltas are different, update accordingly
        std::array<bool, 4> update_loop_filter_ref_deltas;
        std::array<bool, 2> update_loop_filter_mode_deltas;

        bool loop_filter_delta_update = false;

        for (std::size_t index = 0; index < current_frame_info.ref_deltas.size(); index++) {
            const s8 old_deltas = loop_filter_ref_deltas[index];
            const s8 new_deltas = current_frame_info.ref_deltas[index];

            loop_filter_delta_update |=
                (update_loop_filter_ref_deltas[index] = old_deltas != new_deltas);
        }

        for (std::size_t index = 0; index < current_frame_info.mode_deltas.size(); index++) {
            const s8 old_deltas = loop_filter_mode_deltas[index];
            const s8 new_deltas = current_frame_info.mode_deltas[index];

            loop_filter_delta_update |=
                (update_loop_filter_mode_deltas[index] = old_deltas != new_deltas);
        }

        uncomp_writer.WriteBit(loop_filter_delta_update);

        if (loop_filter_delta_update) {
            for (std::size_t index = 0; index < current_frame_info.ref_deltas.size(); index++) {
                uncomp_writer.WriteBit(update_loop_filter_ref_deltas[index]);

                if (update_loop_filter_ref_deltas[index]) {
                    uncomp_writer.WriteS(current_frame_info.ref_deltas[index], 6);
                }
            }

            for (std::size_t index = 0; index < current_frame_info.mode_deltas.size(); index++) {
                uncomp_writer.WriteBit(update_loop_filter_mode_deltas[index]);

                if (update_loop_filter_mode_deltas[index]) {
                    uncomp_writer.WriteS(current_frame_info.mode_deltas[index], 6);
                }
            }
            // save new deltas
            loop_filter_ref_deltas = current_frame_info.ref_deltas;
            loop_filter_mode_deltas = current_frame_info.mode_deltas;
        }
    }

    uncomp_writer.WriteU(current_frame_info.base_q_index, 8);

    uncomp_writer.WriteDeltaQ(current_frame_info.y_dc_delta_q);
    uncomp_writer.WriteDeltaQ(current_frame_info.uv_dc_delta_q);
    uncomp_writer.WriteDeltaQ(current_frame_info.uv_ac_delta_q);

    uncomp_writer.WriteBit(false); // Segmentation enabled (TODO).

    const s32 min_tile_cols_log2 = CalcMinLog2TileCols(current_frame_info.frame_size.width);
    const s32 max_tile_cols_log2 = CalcMaxLog2TileCols(current_frame_info.frame_size.width);

    const s32 tile_cols_log2_diff = current_frame_info.log2_tile_cols - min_tile_cols_log2;
    const s32 tile_cols_log2_inc_mask = (1 << tile_cols_log2_diff) - 1;

    // If it's less than the maximum, we need to add an extra 0 on the bitstream
    // to indicate that it should stop reading.
    if (current_frame_info.log2_tile_cols < max_tile_cols_log2) {
        uncomp_writer.WriteU(tile_cols_log2_inc_mask << 1, tile_cols_log2_diff + 1);
    } else {
        uncomp_writer.WriteU(tile_cols_log2_inc_mask, tile_cols_log2_diff);
    }

    const bool tile_rows_log2_is_nonzero = current_frame_info.log2_tile_rows != 0;

    uncomp_writer.WriteBit(tile_rows_log2_is_nonzero);

    if (tile_rows_log2_is_nonzero) {
        uncomp_writer.WriteBit(current_frame_info.log2_tile_rows > 1);
    }

    return uncomp_writer;
}

/// Composes the VP9 compressed and uncompressed headers from the GPU state information.
/// Based around the official VP9 spec documentation
std::vector<u8>& VP9::ComposeFrameHeader(NvdecCommon::NvdecRegisters& state) {
    // while (true) {
    //     printf("sizeof back upd%ul\n", sizeof(Vp9BackwardUpdates));
    // }
    std::vector<u8> bitstream;
    {
        Vp9FrameContainer curr_frame = GetCurrentFrame(state);
        current_frame_info = curr_frame.info;
        bitstream = curr_frame.bit_stream;
    }

    // The uncompressed header routine sets PrevProb parameters needed for the compressed header
    auto uncomp_writer = ComposeUncompressedHeader();
    std::vector<u8> compressed_header = ComposeCompressedHeader();

    uncomp_writer.WriteU(static_cast<s32>(compressed_header.size()), 16);
    uncomp_writer.Flush();
    std::vector<u8> uncompressed_header = uncomp_writer.GetByteArray();

    // Write headers and frame to buffer
    frame.resize(uncompressed_header.size() + compressed_header.size() + bitstream.size());
    std::memcpy(frame.data(), uncompressed_header.data(), uncompressed_header.size());
    std::memcpy(frame.data() + uncompressed_header.size(), compressed_header.data(),
                compressed_header.size());
    std::memcpy(frame.data() + uncompressed_header.size() + compressed_header.size(),
                bitstream.data(), bitstream.size());

    // keep track of frame number
    current_frame_number++;
    grace_period--;

    // don't display hidden frames
    hidden = !current_frame_info.show_frame;
    return frame;
}

namespace Util {
Stream::Stream() = default;
Stream::~Stream() = default;

void Stream::Seek(s32 cursor, s32 origin) {
    if (origin == SEEK_SET) {
        if (cursor < 0) {
            position = 0;
        } else if (position >= buffer.size()) {
            position = buffer.size();
        }
    } else if (origin == SEEK_CUR) {
        Seek(static_cast<s32>(position) + cursor, SEEK_SET);
    } else if (origin == SEEK_END) {
        Seek(static_cast<s32>(buffer.size()) - cursor, SEEK_SET);
    }
}

u32 Stream::ReadByte() {
    if (position < buffer.size()) {
        return buffer[position];
        position++;
    } else {
        return 0xff;
    }
}

void Stream::WriteByte(u32 byte) {
    const auto u8_byte = static_cast<u8>(byte);
    if (position == buffer.size()) {
        buffer.push_back(u8_byte);
        position++;
    } else {
        buffer.insert(buffer.begin() + position, u8_byte);
    }
}

} // namespace Util

VpxRangeEncoder::VpxRangeEncoder() {
    Write(false);
}

VpxRangeEncoder::~VpxRangeEncoder() = default;

void VpxRangeEncoder::WriteByte(u32 value) {
    Write(static_cast<s32>(value), 8);
}

void VpxRangeEncoder::Write(s32 value, s32 valueSize) {
    for (int bit = valueSize - 1; bit >= 0; bit--) {
        Write(((value >> bit) & 1) != 0);
    }
}

void VpxRangeEncoder::Write(bool bit) {
    Write(bit, half_probability);
}

void VpxRangeEncoder::Write(bool bit, s32 probability) {
    u32 local_range = range;
    const u32 split = 1 + (((local_range - 1) * (u32)probability) >> 8);
    local_range = split;

    if (bit) {
        low_value += split;
        local_range = range - split;
    }

    int shift = norm_lut[local_range];
    local_range <<= shift;
    count += shift;

    if (count >= 0) {
        int offset = shift - count;

        if (((low_value << (offset - 1)) >> 31) != 0) {
            const s32 current_pos = static_cast<s32>(base_stream.GetPosition());
            base_stream.Seek(-1, SEEK_CUR);
            while (base_stream.GetPosition() >= 0 && PeekByte() == 0xff) {
                base_stream.WriteByte(0);

                base_stream.Seek(-2, SEEK_CUR);
            }
            base_stream.WriteByte(static_cast<u8>((PeekByte() + 1)));
            base_stream.Seek(current_pos, SEEK_SET);
        }
        base_stream.WriteByte(static_cast<u8>((low_value >> (24 - offset))));

        low_value <<= offset;
        shift = count;
        low_value &= 0xffffff;
        count -= 8;
    }

    low_value <<= shift;
    range = local_range;
}

void VpxRangeEncoder::End() {
    for (int index = 0; index < 32; index++) {
        Write(false);
    }
}

u8 VpxRangeEncoder::PeekByte() {
    u8 value = static_cast<u8>(base_stream.ReadByte());
    base_stream.Seek(-1, SEEK_CUR);

    return value;
}

VpxBitStreamWriter::VpxBitStreamWriter() = default;

VpxBitStreamWriter::~VpxBitStreamWriter() = default;

void VpxBitStreamWriter::WriteU(s32 value, s32 valueSize) {
    WriteBits(value, valueSize);
}

void VpxBitStreamWriter::WriteS(s32 value, s32 valueSize) {
    const bool sign = value < 0;
    if (sign) {
        value = -value;
    }

    WriteBits((value << 1) | (sign ? 1 : 0), valueSize + 1);
}

void VpxBitStreamWriter::WriteDeltaQ(s32 value) {
    const bool delta_coded = value != 0;
    WriteBit(delta_coded);

    if (delta_coded) {
        WriteBits(value, 4);
    }
}

void VpxBitStreamWriter::WriteBits(s32 value, s32 bit_count) {
    int value_pos = 0;
    int remaining = bit_count;

    while (remaining > 0) {
        int copy_size = remaining;

        const int free = GetFreeBufferBits();

        if (copy_size > free) {
            copy_size = free;
        }

        const int mask = (1 << copy_size) - 1;

        const int src_shift = (bit_count - value_pos) - copy_size;
        const int dst_shift = (buffer_size - buffer_pos) - copy_size;

        buffer |= ((value >> src_shift) & mask) << dst_shift;

        value_pos += copy_size;
        buffer_pos += copy_size;
        remaining -= copy_size;
    }
}

void VpxBitStreamWriter::WriteBit(bool state) {
    WriteBits(state ? 1 : 0, 1);
}

s32 VpxBitStreamWriter::GetFreeBufferBits() {
    if (buffer_pos == buffer_size) {
        Flush();
    }

    return buffer_size - buffer_pos;
}

void VpxBitStreamWriter::Flush() {
    if (buffer_pos == 0) {
        return;
    }
    byte_array.push_back(static_cast<u8>(buffer));
    buffer = 0;
    buffer_pos = 0;
}

std::vector<u8>& VpxBitStreamWriter::GetByteArray() {
    return byte_array;
}

} // namespace Tegra::Decoder
