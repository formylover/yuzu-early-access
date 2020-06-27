// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>

#include <tsl/robin_map.h>

#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"

namespace OpenGL {

constexpr std::size_t BitsPerAttachment = 4;

struct FramebufferCacheKey {
    View zeta;
    std::array<View, Tegra::Engines::Maxwell3D::Regs::NumRenderTargets> colors;
    u32 color_attachments = 0;

    std::size_t Hash() const noexcept;

    bool operator==(const FramebufferCacheKey& rhs) const noexcept;

    bool operator!=(const FramebufferCacheKey& rhs) const noexcept {
        return !operator==(rhs);
    }

    void SetAttachment(std::size_t index, u32 attachment) {
        color_attachments |= attachment << (BitsPerAttachment * index);
    }
};

} // namespace OpenGL

namespace std {

template <>
struct hash<OpenGL::FramebufferCacheKey> {
    std::size_t operator()(const OpenGL::FramebufferCacheKey& k) const noexcept {
        return k.Hash();
    }
};

} // namespace std

namespace OpenGL {

class FramebufferCacheOpenGL {
public:
    FramebufferCacheOpenGL();
    ~FramebufferCacheOpenGL();

    GLuint GetFramebuffer(const FramebufferCacheKey& key);

private:
    OGLFramebuffer CreateFramebuffer(const FramebufferCacheKey& key);

    tsl::robin_map<FramebufferCacheKey, OGLFramebuffer, std::hash<FramebufferCacheKey>,
                   std::equal_to<FramebufferCacheKey>,
                   std::allocator<std::pair<FramebufferCacheKey, OGLFramebuffer>>, true>
        cache;
};

} // namespace OpenGL
