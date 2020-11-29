// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#version 450
#extension GL_ARB_shader_stencil_export : require

layout(binding = 0) uniform sampler2D depth_tex;
layout(binding = 1) uniform isampler2D stencil_tex;

layout(location = 0) in vec2 texcoord;

void main() {
    gl_FragDepth = texture(depth_tex, texcoord).r;
    gl_FragStencilRefARB = texture(stencil_tex, texcoord).r;
}
