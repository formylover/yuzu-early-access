// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <optional>
#include <vector>
#include "common/common_types.h"
#include "video_core/macro/macro.h"

namespace Tegra {

namespace Engines {
class Maxwell3D;
}

using HLEFunction = void (*)(Engines::Maxwell3D& maxwell3d, std::span<const u32> parameters);

class HLEMacro {
public:
    explicit HLEMacro(Engines::Maxwell3D& maxwell3d);
    ~HLEMacro();

    std::optional<std::unique_ptr<CachedMacro>> GetHLEProgram(u64 hash) const;

private:
    Engines::Maxwell3D& maxwell3d;
};

class HLEMacroImpl : public CachedMacro {
public:
    explicit HLEMacroImpl(Engines::Maxwell3D& maxwell3d, HLEFunction func);
    ~HLEMacroImpl();

    void Execute(std::span<const u32> parameters, u32 method) override;

private:
    Engines::Maxwell3D& maxwell3d;
    HLEFunction func;
};

} // namespace Tegra
