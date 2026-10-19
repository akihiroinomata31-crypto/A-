#pragma once
#include <cstdio>
#include <algorithm>
#include <vector>
#include <windows.h>

// A local, once-per-second trace of the actual interactive launch. No on-screen text.
class RuntimeProfile {
    FILE* file_ = nullptr;
    long long last_ = 0, period_ = 0;
    int ticks_ = 0, frames_ = 0;
    double update_ = 0, render_ = 0, present_ = 0;
    std::vector<double> intervals_;
public:
    RuntimeProfile() {
        CreateDirectoryW(L"..\\.merge-review", nullptr);
        fopen_s(&file_, "..\\.merge-review\\runtime-profile.csv", "w");
        if (!file_) return;
        wchar_t path[32768] = {};
        GetModuleFileNameW(nullptr, path, 32768);
        char utf8[32768] = {};
        WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof(utf8), nullptr, nullptr);
#ifdef _DEBUG
        const char* build = "Debug";
#else
        const char* build = "Release";
#endif
        fprintf(file_, "executable,%s\nbuild,%s\npid,%lu\ncompiled,%s %s\n", utf8, build, GetCurrentProcessId(), __DATE__, __TIME__);
        fprintf(file_, "wallSeconds,fps,ticksPerSecond,intervalP95ms,intervalMaxms,updateMs,renderMs,presentMs,activeEnemies,effectInstances,dxMB,debugger,foreground\n");
        fflush(file_);
        intervals_.reserve(256);
    }
    ~RuntimeProfile() { if (file_) fclose(file_); }
    void Reset(long long now) { last_ = period_ = now; frames_ = ticks_ = 0; update_ = render_ = present_ = 0; intervals_.clear(); }
    void Frame(long long start, long long render, long long present, long long now, int ticks, int enemies, int instances, size_t dxBytes) {
        if (!file_) return;
        if (!period_) { Reset(now); return; }
        intervals_.push_back((now - last_) / 1000.0);
        last_ = now; ++frames_; ticks_ += ticks;
        update_ += (render - start) / 1000.0;
        render_ += (present - render) / 1000.0;
        present_ += (now - present) / 1000.0;
        const double seconds = (now - period_) / 1000000.0;
        if (seconds < 1.0) return;
        std::sort(intervals_.begin(), intervals_.end());
        HWND foreground = GetForegroundWindow(); DWORD foregroundPid = 0;
        GetWindowThreadProcessId(foreground, &foregroundPid);
        fprintf(file_, "%.3f,%.2f,%.2f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%.2f,%d,%d\n", now / 1000000.0, frames_ / seconds, ticks_ / seconds,
            intervals_[(intervals_.size() - 1) * 95 / 100], intervals_.back(), update_ / frames_, render_ / frames_, present_ / frames_,
            enemies, instances, dxBytes / 1048576.0, IsDebuggerPresent() ? 1 : 0, foregroundPid == GetCurrentProcessId() ? 1 : 0);
        fflush(file_);
        period_ = now; frames_ = ticks_ = 0; update_ = render_ = present_ = 0; intervals_.clear();
    }
};
