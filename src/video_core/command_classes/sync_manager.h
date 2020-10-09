// MIT License
//
// Copyright (c) Ryujinx Team and Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
// associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
// NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#pragma once

#include <mutex>
#include <vector>
#include "common/common_types.h"

namespace Tegra {
class GPU;
struct SyncptIncr {
    u32 id;
    u32 class_id;
    u32 syncpt_id;
    bool complete;

    SyncptIncr(u32 id, u32 syncptId, u32 classId, bool done = false)
        : id(id), class_id(classId), syncpt_id(syncptId), complete(done) {}
};

class SyncptIncrManager {
public:
    explicit SyncptIncrManager(GPU& gpu);
    ~SyncptIncrManager();

    void Increment(u32 id);

    u32 IncrementWhenDone(u32 classId, u32 id);

    void SignalDone(u32 handle);

    void IncrementAllDone();

private:
    std::vector<SyncptIncr> increments;
    std::mutex increment_lock;
    u32 current_id{};

    GPU& gpu;
};

} // namespace Tegra
