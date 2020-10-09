// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include <algorithm>
#include <list>
#include <vector>
#include "common/cityhash.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "video_core/command_classes/nvdec_common.h"

namespace Tegra {
class GPU;

namespace Decoder {
struct Vp9FrameDimensions {
    s16 width;
    s16 height;
    s16 luma_pitch;
    s16 chroma_pitch;
};
static_assert(sizeof(Vp9FrameDimensions) == 0x8, "Vp9 Vp9FrameDimensions is an invalid size");

enum FrameFlags : u32 {
    IsKeyFrame = 1 << 0,
    LastFrameIsKeyFrame = 1 << 1,
    FrameSizeChanged = 1 << 2,
    ErrorResilientMode = 1 << 3,
    LastShowFrame = 1 << 4,
    IntraOnly = 1 << 5,
};

enum class MvJointType {
    MvJointZero = 0,   /* Zero vector */
    MvJointHnzvz = 1,  /* Vert zero, hor nonzero */
    MvJointHzvnz = 2,  /* Hor zero, vert nonzero */
    MvJointHnzvnz = 3, /* Both components nonzero */
};
enum class MvClassType {
    MvClass0 = 0,   /* (0, 2]     integer pel */
    MvClass1 = 1,   /* (2, 4]     integer pel */
    MvClass2 = 2,   /* (4, 8]     integer pel */
    MvClass3 = 3,   /* (8, 16]    integer pel */
    MvClass4 = 4,   /* (16, 32]   integer pel */
    MvClass5 = 5,   /* (32, 64]   integer pel */
    MvClass6 = 6,   /* (64, 128]  integer pel */
    MvClass7 = 7,   /* (128, 256] integer pel */
    MvClass8 = 8,   /* (256, 512] integer pel */
    MvClass9 = 9,   /* (512, 1024] integer pel */
    MvClass10 = 10, /* (1024,2048] integer pel */
};

enum class BlockSize {
    Block4x4 = 0,
    Block4x8 = 1,
    Block8x4 = 2,
    Block8x8 = 3,
    Block8x16 = 4,
    Block16x8 = 5,
    Block16x16 = 6,
    Block16x32 = 7,
    Block32x16 = 8,
    Block32x32 = 9,
    Block32x64 = 10,
    Block64x32 = 11,
    Block64x64 = 12,
    BlockSizes = 13,
    BlockInvalid = BlockSizes
};

enum class PredictionMode {
    DcPred = 0,   // Average of above and left pixels
    VPred = 1,    // Vertical
    HPred = 2,    // Horizontal
    D45Pred = 3,  // Directional 45  deg = round(arctan(1 / 1) * 180 / pi)
    D135Pred = 4, // Directional 135 deg = 180 - 45
    D117Pred = 5, // Directional 117 deg = 180 - 63
    D153Pred = 6, // Directional 153 deg = 180 - 27
    D207Pred = 7, // Directional 207 deg = 180 + 27
    D63Pred = 8,  // Directional 63  deg = round(arctan(2 / 1) * 180 / pi)
    TmPred = 9,   // True-motion
    NearestMv = 10,
    NearMv = 11,
    ZeroMv = 12,
    NewMv = 13,
    MbModeCount = 14
};

enum class TxSize {
    Tx4x4 = 0,   // 4x4 transform
    Tx8x8 = 1,   // 8x8 transform
    Tx16x16 = 2, // 16x16 transform
    Tx32x32 = 3, // 32x32 transform
    TxSizes = 4
};

enum class TxMode {
    Only4X4 = 0,      // Only 4x4 transform used
    Allow8X8 = 1,     // Allow block transform size up to 8x8
    Allow16X16 = 2,   // Allow block transform size up to 16x16
    Allow32X32 = 3,   // Allow block transform size up to 32x32
    TxModeSelect = 4, // Transform specified for each block
    TxModes = 5
};

enum class reference_mode {
    SingleReference = 0,
    CompoundReference = 1,
    ReferenceModeSelect = 2,
    ReferenceModes = 3
};

struct Segmentation {
    u8 Enabled;
    u8 UpdateMap;
    u8 TemporalUpdate;
    u8 AbsDelta;
    std::array<u32, 8> FeatureMask;
    std::array<std::array<s16, 4>, 8> FeatureData;
};
static_assert(sizeof(Segmentation) == 0x64, "Segmentation is an invalid size");

struct LoopFilter {
    u8 mode_ref_delta_enabled;
    std::array<s8, 4> ref_deltas;
    std::array<s8, 2> mode_deltas;
};
static_assert(sizeof(LoopFilter) == 0x7, "LoopFilter is an invalid size");

struct Vp9EntropyProbs {
    std::array<u8, 36> y_mode_prob{};
    std::array<u8, 64> partition_prob{};
    std::array<u8, 2304> coef_probs{};
    std::array<u8, 8> switchable_interp_prob{};
    std::array<u8, 28> inter_mode_prob{};
    std::array<u8, 4> intra_inter_prob{};
    std::array<u8, 5> comp_inter_prob{};
    std::array<u8, 10> single_ref_prob{};
    std::array<u8, 5> comp_ref_prob{};
    std::array<u8, 6> tx_32x32_prob{};
    std::array<u8, 4> tx_16x16_prob{};
    std::array<u8, 2> tx_8x8_prob{};
    std::array<u8, 3> skip_probs{};
    std::array<u8, 3> joints{};
    std::array<u8, 2> sign{};
    std::array<u8, 20> classes{};
    std::array<u8, 2> class_0{};
    std::array<u8, 20> prob_bits{};
    std::array<u8, 12> class_0_fr{};
    std::array<u8, 6> fr{};
    std::array<u8, 2> class_0_hp{};
    std::array<u8, 2> high_precision{};
};

// Default probabilities once frame context resets
static constexpr Vp9EntropyProbs default_probs{
    .y_mode_prob{
        65,  32, 18, 144, 162, 194, 41, 51, 98, 132, 68,  18, 165, 217, 196, 45, 40, 78,
        173, 80, 19, 176, 240, 193, 64, 35, 46, 221, 135, 38, 194, 248, 121, 96, 85, 29,
    },
    .partition_prob{
        199, 122, 141, 0, 147, 63, 159, 0, 148, 133, 118, 0, 121, 104, 114, 0,
        174, 73,  87,  0, 92,  41, 83,  0, 82,  99,  50,  0, 53,  39,  39,  0,
        177, 58,  59,  0, 68,  26, 63,  0, 52,  79,  25,  0, 17,  14,  12,  0,
        222, 34,  30,  0, 72,  16, 44,  0, 58,  32,  12,  0, 10,  7,   6,   0,
    },
    .coef_probs{
        195, 29,  183, 0, 84,  49,  136, 0, 8,   42,  71,  0, 0,   0,   0,   0, 0,   0,   0,   0,
        0,   0,   0,   0, 31,  107, 169, 0, 35,  99,  159, 0, 17,  82,  140, 0, 8,   66,  114, 0,
        2,   44,  76,  0, 1,   19,  32,  0, 40,  132, 201, 0, 29,  114, 187, 0, 13,  91,  157, 0,
        7,   75,  127, 0, 3,   58,  95,  0, 1,   28,  47,  0, 69,  142, 221, 0, 42,  122, 201, 0,
        15,  91,  159, 0, 6,   67,  121, 0, 1,   42,  77,  0, 1,   17,  31,  0, 102, 148, 228, 0,
        67,  117, 204, 0, 17,  82,  154, 0, 6,   59,  114, 0, 2,   39,  75,  0, 1,   15,  29,  0,
        156, 57,  233, 0, 119, 57,  212, 0, 58,  48,  163, 0, 29,  40,  124, 0, 12,  30,  81,  0,
        3,   12,  31,  0, 191, 107, 226, 0, 124, 117, 204, 0, 25,  99,  155, 0, 0,   0,   0,   0,
        0,   0,   0,   0, 0,   0,   0,   0, 29,  148, 210, 0, 37,  126, 194, 0, 8,   93,  157, 0,
        2,   68,  118, 0, 1,   39,  69,  0, 1,   17,  33,  0, 41,  151, 213, 0, 27,  123, 193, 0,
        3,   82,  144, 0, 1,   58,  105, 0, 1,   32,  60,  0, 1,   13,  26,  0, 59,  159, 220, 0,
        23,  126, 198, 0, 4,   88,  151, 0, 1,   66,  114, 0, 1,   38,  71,  0, 1,   18,  34,  0,
        114, 136, 232, 0, 51,  114, 207, 0, 11,  83,  155, 0, 3,   56,  105, 0, 1,   33,  65,  0,
        1,   17,  34,  0, 149, 65,  234, 0, 121, 57,  215, 0, 61,  49,  166, 0, 28,  36,  114, 0,
        12,  25,  76,  0, 3,   16,  42,  0, 214, 49,  220, 0, 132, 63,  188, 0, 42,  65,  137, 0,
        0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0, 85,  137, 221, 0, 104, 131, 216, 0,
        49,  111, 192, 0, 21,  87,  155, 0, 2,   49,  87,  0, 1,   16,  28,  0, 89,  163, 230, 0,
        90,  137, 220, 0, 29,  100, 183, 0, 10,  70,  135, 0, 2,   42,  81,  0, 1,   17,  33,  0,
        108, 167, 237, 0, 55,  133, 222, 0, 15,  97,  179, 0, 4,   72,  135, 0, 1,   45,  85,  0,
        1,   19,  38,  0, 124, 146, 240, 0, 66,  124, 224, 0, 17,  88,  175, 0, 4,   58,  122, 0,
        1,   36,  75,  0, 1,   18,  37,  0, 141, 79,  241, 0, 126, 70,  227, 0, 66,  58,  182, 0,
        30,  44,  136, 0, 12,  34,  96,  0, 2,   20,  47,  0, 229, 99,  249, 0, 143, 111, 235, 0,
        46,  109, 192, 0, 0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0, 82,  158, 236, 0,
        94,  146, 224, 0, 25,  117, 191, 0, 9,   87,  149, 0, 3,   56,  99,  0, 1,   33,  57,  0,
        83,  167, 237, 0, 68,  145, 222, 0, 10,  103, 177, 0, 2,   72,  131, 0, 1,   41,  79,  0,
        1,   20,  39,  0, 99,  167, 239, 0, 47,  141, 224, 0, 10,  104, 178, 0, 2,   73,  133, 0,
        1,   44,  85,  0, 1,   22,  47,  0, 127, 145, 243, 0, 71,  129, 228, 0, 17,  93,  177, 0,
        3,   61,  124, 0, 1,   41,  84,  0, 1,   21,  52,  0, 157, 78,  244, 0, 140, 72,  231, 0,
        69,  58,  184, 0, 31,  44,  137, 0, 14,  38,  105, 0, 8,   23,  61,  0, 125, 34,  187, 0,
        52,  41,  133, 0, 6,   31,  56,  0, 0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0,
        37,  109, 153, 0, 51,  102, 147, 0, 23,  87,  128, 0, 8,   67,  101, 0, 1,   41,  63,  0,
        1,   19,  29,  0, 31,  154, 185, 0, 17,  127, 175, 0, 6,   96,  145, 0, 2,   73,  114, 0,
        1,   51,  82,  0, 1,   28,  45,  0, 23,  163, 200, 0, 10,  131, 185, 0, 2,   93,  148, 0,
        1,   67,  111, 0, 1,   41,  69,  0, 1,   14,  24,  0, 29,  176, 217, 0, 12,  145, 201, 0,
        3,   101, 156, 0, 1,   69,  111, 0, 1,   39,  63,  0, 1,   14,  23,  0, 57,  192, 233, 0,
        25,  154, 215, 0, 6,   109, 167, 0, 3,   78,  118, 0, 1,   48,  69,  0, 1,   21,  29,  0,
        202, 105, 245, 0, 108, 106, 216, 0, 18,  90,  144, 0, 0,   0,   0,   0, 0,   0,   0,   0,
        0,   0,   0,   0, 33,  172, 219, 0, 64,  149, 206, 0, 14,  117, 177, 0, 5,   90,  141, 0,
        2,   61,  95,  0, 1,   37,  57,  0, 33,  179, 220, 0, 11,  140, 198, 0, 1,   89,  148, 0,
        1,   60,  104, 0, 1,   33,  57,  0, 1,   12,  21,  0, 30,  181, 221, 0, 8,   141, 198, 0,
        1,   87,  145, 0, 1,   58,  100, 0, 1,   31,  55,  0, 1,   12,  20,  0, 32,  186, 224, 0,
        7,   142, 198, 0, 1,   86,  143, 0, 1,   58,  100, 0, 1,   31,  55,  0, 1,   12,  22,  0,
        57,  192, 227, 0, 20,  143, 204, 0, 3,   96,  154, 0, 1,   68,  112, 0, 1,   42,  69,  0,
        1,   19,  32,  0, 212, 35,  215, 0, 113, 47,  169, 0, 29,  48,  105, 0, 0,   0,   0,   0,
        0,   0,   0,   0, 0,   0,   0,   0, 74,  129, 203, 0, 106, 120, 203, 0, 49,  107, 178, 0,
        19,  84,  144, 0, 4,   50,  84,  0, 1,   15,  25,  0, 71,  172, 217, 0, 44,  141, 209, 0,
        15,  102, 173, 0, 6,   76,  133, 0, 2,   51,  89,  0, 1,   24,  42,  0, 64,  185, 231, 0,
        31,  148, 216, 0, 8,   103, 175, 0, 3,   74,  131, 0, 1,   46,  81,  0, 1,   18,  30,  0,
        65,  196, 235, 0, 25,  157, 221, 0, 5,   105, 174, 0, 1,   67,  120, 0, 1,   38,  69,  0,
        1,   15,  30,  0, 65,  204, 238, 0, 30,  156, 224, 0, 7,   107, 177, 0, 2,   70,  124, 0,
        1,   42,  73,  0, 1,   18,  34,  0, 225, 86,  251, 0, 144, 104, 235, 0, 42,  99,  181, 0,
        0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0, 85,  175, 239, 0, 112, 165, 229, 0,
        29,  136, 200, 0, 12,  103, 162, 0, 6,   77,  123, 0, 2,   53,  84,  0, 75,  183, 239, 0,
        30,  155, 221, 0, 3,   106, 171, 0, 1,   74,  128, 0, 1,   44,  76,  0, 1,   17,  28,  0,
        73,  185, 240, 0, 27,  159, 222, 0, 2,   107, 172, 0, 1,   75,  127, 0, 1,   42,  73,  0,
        1,   17,  29,  0, 62,  190, 238, 0, 21,  159, 222, 0, 2,   107, 172, 0, 1,   72,  122, 0,
        1,   40,  71,  0, 1,   18,  32,  0, 61,  199, 240, 0, 27,  161, 226, 0, 4,   113, 180, 0,
        1,   76,  129, 0, 1,   46,  80,  0, 1,   23,  41,  0, 7,   27,  153, 0, 5,   30,  95,  0,
        1,   16,  30,  0, 0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0, 50,  75,  127, 0,
        57,  75,  124, 0, 27,  67,  108, 0, 10,  54,  86,  0, 1,   33,  52,  0, 1,   12,  18,  0,
        43,  125, 151, 0, 26,  108, 148, 0, 7,   83,  122, 0, 2,   59,  89,  0, 1,   38,  60,  0,
        1,   17,  27,  0, 23,  144, 163, 0, 13,  112, 154, 0, 2,   75,  117, 0, 1,   50,  81,  0,
        1,   31,  51,  0, 1,   14,  23,  0, 18,  162, 185, 0, 6,   123, 171, 0, 1,   78,  125, 0,
        1,   51,  86,  0, 1,   31,  54,  0, 1,   14,  23,  0, 15,  199, 227, 0, 3,   150, 204, 0,
        1,   91,  146, 0, 1,   55,  95,  0, 1,   30,  53,  0, 1,   11,  20,  0, 19,  55,  240, 0,
        19,  59,  196, 0, 3,   52,  105, 0, 0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0,
        41,  166, 207, 0, 104, 153, 199, 0, 31,  123, 181, 0, 14,  101, 152, 0, 5,   72,  106, 0,
        1,   36,  52,  0, 35,  176, 211, 0, 12,  131, 190, 0, 2,   88,  144, 0, 1,   60,  101, 0,
        1,   36,  60,  0, 1,   16,  28,  0, 28,  183, 213, 0, 8,   134, 191, 0, 1,   86,  142, 0,
        1,   56,  96,  0, 1,   30,  53,  0, 1,   12,  20,  0, 20,  190, 215, 0, 4,   135, 192, 0,
        1,   84,  139, 0, 1,   53,  91,  0, 1,   28,  49,  0, 1,   11,  20,  0, 13,  196, 216, 0,
        2,   137, 192, 0, 1,   86,  143, 0, 1,   57,  99,  0, 1,   32,  56,  0, 1,   13,  24,  0,
        211, 29,  217, 0, 96,  47,  156, 0, 22,  43,  87,  0, 0,   0,   0,   0, 0,   0,   0,   0,
        0,   0,   0,   0, 78,  120, 193, 0, 111, 116, 186, 0, 46,  102, 164, 0, 15,  80,  128, 0,
        2,   49,  76,  0, 1,   18,  28,  0, 71,  161, 203, 0, 42,  132, 192, 0, 10,  98,  150, 0,
        3,   69,  109, 0, 1,   44,  70,  0, 1,   18,  29,  0, 57,  186, 211, 0, 30,  140, 196, 0,
        4,   93,  146, 0, 1,   62,  102, 0, 1,   38,  65,  0, 1,   16,  27,  0, 47,  199, 217, 0,
        14,  145, 196, 0, 1,   88,  142, 0, 1,   57,  98,  0, 1,   36,  62,  0, 1,   15,  26,  0,
        26,  219, 229, 0, 5,   155, 207, 0, 1,   94,  151, 0, 1,   60,  104, 0, 1,   36,  62,  0,
        1,   16,  28,  0, 233, 29,  248, 0, 146, 47,  220, 0, 43,  52,  140, 0, 0,   0,   0,   0,
        0,   0,   0,   0, 0,   0,   0,   0, 100, 163, 232, 0, 179, 161, 222, 0, 63,  142, 204, 0,
        37,  113, 174, 0, 26,  89,  137, 0, 18,  68,  97,  0, 85,  181, 230, 0, 32,  146, 209, 0,
        7,   100, 164, 0, 3,   71,  121, 0, 1,   45,  77,  0, 1,   18,  30,  0, 65,  187, 230, 0,
        20,  148, 207, 0, 2,   97,  159, 0, 1,   68,  116, 0, 1,   40,  70,  0, 1,   14,  29,  0,
        40,  194, 227, 0, 8,   147, 204, 0, 1,   94,  155, 0, 1,   65,  112, 0, 1,   39,  66,  0,
        1,   14,  26,  0, 16,  208, 228, 0, 3,   151, 207, 0, 1,   98,  160, 0, 1,   67,  117, 0,
        1,   41,  74,  0, 1,   17,  31,  0, 17,  38,  140, 0, 7,   34,  80,  0, 1,   17,  29,  0,
        0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0, 37,  75,  128, 0, 41,  76,  128, 0,
        26,  66,  116, 0, 12,  52,  94,  0, 2,   32,  55,  0, 1,   10,  16,  0, 50,  127, 154, 0,
        37,  109, 152, 0, 16,  82,  121, 0, 5,   59,  85,  0, 1,   35,  54,  0, 1,   13,  20,  0,
        40,  142, 167, 0, 17,  110, 157, 0, 2,   71,  112, 0, 1,   44,  72,  0, 1,   27,  45,  0,
        1,   11,  17,  0, 30,  175, 188, 0, 9,   124, 169, 0, 1,   74,  116, 0, 1,   48,  78,  0,
        1,   30,  49,  0, 1,   11,  18,  0, 10,  222, 223, 0, 2,   150, 194, 0, 1,   83,  128, 0,
        1,   48,  79,  0, 1,   27,  45,  0, 1,   11,  17,  0, 36,  41,  235, 0, 29,  36,  193, 0,
        10,  27,  111, 0, 0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0, 85,  165, 222, 0,
        177, 162, 215, 0, 110, 135, 195, 0, 57,  113, 168, 0, 23,  83,  120, 0, 10,  49,  61,  0,
        85,  190, 223, 0, 36,  139, 200, 0, 5,   90,  146, 0, 1,   60,  103, 0, 1,   38,  65,  0,
        1,   18,  30,  0, 72,  202, 223, 0, 23,  141, 199, 0, 2,   86,  140, 0, 1,   56,  97,  0,
        1,   36,  61,  0, 1,   16,  27,  0, 55,  218, 225, 0, 13,  145, 200, 0, 1,   86,  141, 0,
        1,   57,  99,  0, 1,   35,  61,  0, 1,   13,  22,  0, 15,  235, 212, 0, 1,   132, 184, 0,
        1,   84,  139, 0, 1,   57,  97,  0, 1,   34,  56,  0, 1,   14,  23,  0, 181, 21,  201, 0,
        61,  37,  123, 0, 10,  38,  71,  0, 0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0,
        47,  106, 172, 0, 95,  104, 173, 0, 42,  93,  159, 0, 18,  77,  131, 0, 4,   50,  81,  0,
        1,   17,  23,  0, 62,  147, 199, 0, 44,  130, 189, 0, 28,  102, 154, 0, 18,  75,  115, 0,
        2,   44,  65,  0, 1,   12,  19,  0, 55,  153, 210, 0, 24,  130, 194, 0, 3,   93,  146, 0,
        1,   61,  97,  0, 1,   31,  50,  0, 1,   10,  16,  0, 49,  186, 223, 0, 17,  148, 204, 0,
        1,   96,  142, 0, 1,   53,  83,  0, 1,   26,  44,  0, 1,   11,  17,  0, 13,  217, 212, 0,
        2,   136, 180, 0, 1,   78,  124, 0, 1,   50,  83,  0, 1,   29,  49,  0, 1,   14,  23,  0,
        197, 13,  247, 0, 82,  17,  222, 0, 25,  17,  162, 0, 0,   0,   0,   0, 0,   0,   0,   0,
        0,   0,   0,   0, 126, 186, 247, 0, 234, 191, 243, 0, 176, 177, 234, 0, 104, 158, 220, 0,
        66,  128, 186, 0, 55,  90,  137, 0, 111, 197, 242, 0, 46,  158, 219, 0, 9,   104, 171, 0,
        2,   65,  125, 0, 1,   44,  80,  0, 1,   17,  91,  0, 104, 208, 245, 0, 39,  168, 224, 0,
        3,   109, 162, 0, 1,   79,  124, 0, 1,   50,  102, 0, 1,   43,  102, 0, 84,  220, 246, 0,
        31,  177, 231, 0, 2,   115, 180, 0, 1,   79,  134, 0, 1,   55,  77,  0, 1,   60,  79,  0,
        43,  243, 240, 0, 8,   180, 217, 0, 1,   115, 166, 0, 1,   84,  121, 0, 1,   51,  67,  0,
        1,   16,  6,   0,
    },
    .switchable_interp_prob{235, 162, 36, 255, 34, 3, 149, 144},
    .inter_mode_prob{
        2,  173, 34, 0,  7,  145, 85, 0,  7,  166, 63, 0,  7,  94,
        66, 0,   8,  64, 46, 0,   17, 81, 31, 0,   25, 29, 30, 0,
    },
    .intra_inter_prob{9, 102, 187, 225},
    .comp_inter_prob{9, 102, 187, 225, 0},
    .single_ref_prob{33, 16, 77, 74, 142, 142, 172, 170, 238, 247},
    .comp_ref_prob{50, 126, 123, 221, 226},
    .tx_32x32_prob{3, 136, 37, 5, 52, 13},
    .tx_16x16_prob{20, 152, 15, 101},
    .tx_8x8_prob{100, 66},
    .skip_probs{192, 128, 64},
    .joints{32, 64, 96},
    .sign{128, 128},
    .classes{
        224, 144, 192, 168, 192, 176, 192, 198, 198, 245,
        216, 128, 176, 160, 176, 176, 192, 198, 198, 208,
    },
    .class_0{216, 208},
    .prob_bits{
        136, 140, 148, 160, 176, 192, 224, 234, 234, 240,
        136, 140, 148, 160, 176, 192, 224, 234, 234, 240,
    },
    .class_0_fr{128, 128, 64, 96, 112, 64, 128, 128, 64, 96, 112, 64},
    .fr{64, 96, 64, 64, 96, 64},
    .class_0_hp{160, 160},
    .high_precision{128, 128},
};

struct Vp9PictureInfo {
    bool is_key_frame;
    bool intra_only;
    bool last_frame_was_key;
    bool frame_size_changed;
    bool error_resilient_mode;
    bool last_frame_shown;
    bool show_frame;
    std::array<s8, 4> ref_frame_sign_bias;
    int base_q_index;
    int y_dc_delta_q;
    int uv_dc_delta_q;
    int uv_ac_delta_q;
    bool lossless;
    int transform_mode;
    bool allow_high_precision_mv;
    int interp_filter;
    int reference_mode;
    s8 comp_fixed_ref;
    std::array<s8, 2> comp_var_ref;
    int log2_tile_cols;
    int log2_tile_rows;
    bool segment_enabled;
    bool segment_map_update;
    bool segment_map_temporal_update;
    int segment_abs_delta;
    std::array<u32, 8> segment_feature_enable;
    std::array<std::array<s16, 4>, 8> segment_feature_data;
    bool mode_ref_delta_enabled;
    bool use_prev_in_find_mv_refs;
    std::array<s8, 4> ref_deltas;
    std::array<s8, 2> mode_deltas;
    Vp9EntropyProbs entropy{};
    INSERT_PADDING_BYTES(0x3398);
    Vp9FrameDimensions frame_size{};
    u8 first_level{};
    u8 sharpness_level{};
    u32 bitstream_size{};
    std::array<u32, 4> frame_offsets{};
    std::array<bool, 4> refresh_frame{};
};

struct Vp9FrameContainer {
    Vp9PictureInfo info{};
    std::vector<u8> bit_stream;
};

struct PictureInfo {
    INSERT_PADDING_WORDS(12);
    u32 bitstream_size;
    INSERT_PADDING_WORDS(5);
    Vp9FrameDimensions last_frame_size;
    Vp9FrameDimensions golden_frame_size;
    Vp9FrameDimensions alt_frame_size;
    Vp9FrameDimensions current_frame_size;
    u32 vp9_flags;
    std::array<s8, 4> ref_frame_sign_bias;
    u8 first_level;
    u8 sharpness_level;
    u8 base_q_index;
    u8 y_dc_delta_q;
    u8 uv_ac_delta_q;
    u8 uv_dc_delta_q;
    u8 lossless;
    u8 tx_mode;
    u8 allow_high_precision_mv;
    u8 interp_filter;
    u8 reference_mode;
    s8 comp_fixed_ref;
    std::array<s8, 2> comp_var_ref;
    u8 log2_tile_cols;
    u8 log2_tile_rows;
    Segmentation segmentation;
    LoopFilter loop_filter;
    u8 padding_0;
    u32 padding_1;
    u32 surface_params;
    INSERT_PADDING_WORDS(3);

    u32 bit_depth = (surface_params >> 1) & 0xf;

    Vp9PictureInfo Convert() const {
        return Vp9PictureInfo{
            .is_key_frame = (vp9_flags & FrameFlags::IsKeyFrame) != 0,
            .intra_only = (vp9_flags & FrameFlags::IntraOnly) != 0,
            .last_frame_was_key = (vp9_flags & FrameFlags::LastFrameIsKeyFrame) != 0,
            .frame_size_changed = (vp9_flags & FrameFlags::FrameSizeChanged) != 0,
            .error_resilient_mode = (vp9_flags & FrameFlags::ErrorResilientMode) != 0,
            .last_frame_shown = (vp9_flags & FrameFlags::LastShowFrame) != 0,
            .ref_frame_sign_bias = ref_frame_sign_bias,
            .base_q_index = base_q_index,
            .y_dc_delta_q = y_dc_delta_q,
            .uv_dc_delta_q = uv_dc_delta_q,
            .uv_ac_delta_q = uv_ac_delta_q,
            .lossless = lossless != 0,
            .transform_mode = tx_mode,
            .allow_high_precision_mv = allow_high_precision_mv != 0,
            .interp_filter = interp_filter,
            .reference_mode = reference_mode,
            .comp_fixed_ref = comp_fixed_ref,
            .comp_var_ref = comp_var_ref,
            .log2_tile_cols = log2_tile_cols,
            .log2_tile_rows = log2_tile_rows,
            .segment_enabled = segmentation.Enabled != 0,
            .segment_map_update = segmentation.UpdateMap != 0,
            .segment_map_temporal_update = segmentation.TemporalUpdate != 0,
            .segment_abs_delta = segmentation.AbsDelta,
            .segment_feature_enable = segmentation.FeatureMask,
            .segment_feature_data = segmentation.FeatureData,
            .mode_ref_delta_enabled = loop_filter.mode_ref_delta_enabled != 0,
            .use_prev_in_find_mv_refs = !(vp9_flags == (FrameFlags::ErrorResilientMode)) &&
                                        !(vp9_flags == (FrameFlags::FrameSizeChanged)) &&
                                        !(vp9_flags == (FrameFlags::IntraOnly)) &&
                                        (vp9_flags == (FrameFlags::LastShowFrame)) &&
                                        !(vp9_flags == (FrameFlags::LastFrameIsKeyFrame)),
            .ref_deltas = loop_filter.ref_deltas,
            .mode_deltas = loop_filter.mode_deltas,
            .frame_size = current_frame_size,
            .first_level = first_level,
            .sharpness_level = sharpness_level,
            .bitstream_size = bitstream_size,
        };
    }
};

struct EntropyProbs {
    INSERT_PADDING_BYTES(6 + 10 + 8 * 10 + 15 + 3 + 7 + 3 + 10 * 10 + 8 * 10 * 10);

    std::array<std::array<u8, 4>, 7> inter_mode_prob;
    std::array<u8, 4> intra_inter_prob;
    std::array<std::array<u8, 8>, 10> UvModeProbE0ToE7;
    std::array<std::array<u8, 1>, 2> tx_8x8_prob;
    std::array<std::array<u8, 2>, 2> tx_16x16_prob;
    std::array<std::array<u8, 3>, 2> tx_32x32_prob;
    std::array<u8, 4> y_mode_prob_e8;
    std::array<std::array<u8, 8>, 4> y_mode_prob_e0e7;
    std::array<std::array<u8, 4>, 16> KfPartitionProb;
    std::array<std::array<u8, 4>, 16> partition_prob;
    std::array<u8, 10> UvModeProbE8;
    std::array<std::array<u8, 2>, 4> switchable_interp_prob;
    std::array<u8, 5> comp_inter_prob;
    std::array<u8, 4> skip_probs;
    std::array<u8, 3> joints;
    std::array<u8, 2> sign;
    std::array<std::array<u8, 1>, 2> class_0;
    std::array<std::array<u8, 3>, 2> fr;
    std::array<u8, 2> class_0_hp;
    std::array<u8, 2> high_precision;
    std::array<std::array<u8, 10>, 2> classes;
    std::array<std::array<std::array<u8, 3>, 2>, 2> class_0_fr;
    std::array<std::array<u8, 10>, 2> pred_bits;
    std::array<std::array<u8, 2>, 5> single_ref_prob;
    std::array<u8, 5> comp_ref_prob;
    INSERT_PADDING_BYTES(17);
    std::array<std::array<std::array<std::array<std::array<std::array<u8, 4>, 6>, 6>, 2>, 2>, 4>
        coef_probs;

    void Convert(Vp9EntropyProbs& fc) {
        std::memcpy(fc.inter_mode_prob.data(), inter_mode_prob.data(), fc.inter_mode_prob.size());

        std::memcpy(fc.intra_inter_prob.data(), intra_inter_prob.data(),
                    fc.intra_inter_prob.size());

        std::memcpy(fc.tx_8x8_prob.data(), tx_8x8_prob.data(), fc.tx_8x8_prob.size());
        std::memcpy(fc.tx_16x16_prob.data(), tx_16x16_prob.data(), fc.tx_16x16_prob.size());
        std::memcpy(fc.tx_32x32_prob.data(), tx_32x32_prob.data(), fc.tx_32x32_prob.size());

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 9; j++) {
                fc.y_mode_prob[j + 9 * i] = j < 8 ? y_mode_prob_e0e7[i][j] : y_mode_prob_e8[i];
            }
        }

        std::memcpy(fc.partition_prob.data(), partition_prob.data(), fc.partition_prob.size());

        std::memcpy(fc.switchable_interp_prob.data(), switchable_interp_prob.data(),
                    fc.switchable_interp_prob.size());
        std::memcpy(fc.comp_inter_prob.data(), comp_inter_prob.data(), fc.comp_inter_prob.size());
        std::memcpy(fc.skip_probs.data(), skip_probs.data(), fc.skip_probs.size());

        std::memcpy(fc.joints.data(), joints.data(), fc.joints.size());

        std::memcpy(fc.sign.data(), sign.data(), fc.sign.size());
        std::memcpy(fc.class_0.data(), class_0.data(), fc.class_0.size());
        std::memcpy(fc.fr.data(), fr.data(), fc.fr.size());
        std::memcpy(fc.class_0_hp.data(), class_0_hp.data(), fc.class_0_hp.size());
        std::memcpy(fc.high_precision.data(), high_precision.data(), fc.high_precision.size());
        std::memcpy(fc.classes.data(), classes.data(), fc.classes.size());
        std::memcpy(fc.class_0_fr.data(), class_0_fr.data(), fc.class_0_fr.size());
        std::memcpy(fc.prob_bits.data(), pred_bits.data(), fc.prob_bits.size());
        std::memcpy(fc.single_ref_prob.data(), single_ref_prob.data(), fc.single_ref_prob.size());
        std::memcpy(fc.comp_ref_prob.data(), comp_ref_prob.data(), fc.comp_ref_prob.size());

        std::memcpy(fc.coef_probs.data(), coef_probs.data(), fc.coef_probs.size());
    }
};

enum class Ref { LAST, GOLDEN, ALTREF };

struct RefPoolElement {
    s64 frame;
    Ref ref;
    bool refresh;
};

struct FrameContexts {
    s64 from;
    bool adapted;
    Vp9EntropyProbs probs{};
};

}; // namespace Decoder
}; // namespace Tegra
