/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERBUDGETS_H
#define CWRENDERBUDGETS_H

//Qt includes
#include <QtGlobal>

namespace cw::budgets {
    constexpr qint64 kBytesPerMegabyte = 1024 * 1024;

    // The one home for the streaming budget knobs. cwRenderingSettings stores and
    // clamps them in megabytes; a frame renderer running without that singleton
    // (tests, tools) falls back to the byte defaults through cwRenderBudgets.
    constexpr int kDefaultGpuBudgetMb = 1536;
    constexpr int kMinGpuBudgetMb = 256;
    constexpr int kMaxGpuBudgetMb = 65536;

    constexpr int kDefaultCpuBudgetMb = 512;
    constexpr int kMinCpuBudgetMb = 64;
    constexpr int kMaxCpuBudgetMb = 16384;

    constexpr int kDefaultUploadBudgetMbPerFrame = 8;
    constexpr int kMinUploadBudgetMbPerFrame = 1;
    constexpr int kMaxUploadBudgetMbPerFrame = 256;

    // Points every point cloud in one view may draw together in a frame. Frame
    // time on a point cloud follows the points drawn, and nothing else here is
    // expressed in that unit: 16 M held 60 fps on the profiling Mac.
    constexpr int kDefaultPointBudgetMillions = 16;
    constexpr int kMinPointBudgetMillions = 1;
    constexpr int kMaxPointBudgetMillions = 512;
    constexpr qint64 kPointsPerMillion = 1000000;

    constexpr double kDefaultScreenSpaceErrorPx = 1.5;
    constexpr double kMinScreenSpaceErrorPx = 0.5;
    constexpr double kMaxScreenSpaceErrorPx = 8.0;

    constexpr qint64 kDefaultGpuBudgetBytes = kDefaultGpuBudgetMb * kBytesPerMegabyte;
    constexpr qint64 kDefaultCpuBudgetBytes = kDefaultCpuBudgetMb * kBytesPerMegabyte;
    constexpr qint64 kDefaultUploadBudgetBytesPerFrame = kDefaultUploadBudgetMbPerFrame * kBytesPerMegabyte;
    constexpr qint64 kDefaultPointBudgetPoints = kDefaultPointBudgetMillions * kPointsPerMillion;
}

/**
 * The streaming budget knobs, read from cwRenderingSettings at the sync barrier
 * and stamped onto every RenderData alongside the camera, so the render thread
 * never reaches back across the barrier for them.
 */
struct cwRenderBudgets {
    qint64 gpuBudgetBytes = cw::budgets::kDefaultGpuBudgetBytes;
    qint64 cpuBudgetBytes = cw::budgets::kDefaultCpuBudgetBytes;
    qint64 uploadBudgetBytesPerFrame = cw::budgets::kDefaultUploadBudgetBytesPerFrame;
    double screenSpaceErrorPx = cw::budgets::kDefaultScreenSpaceErrorPx;

    //! Points per frame across every cloud in the view
    qint64 pointBudget = cw::budgets::kDefaultPointBudgetPoints;
};

#endif // CWRENDERBUDGETS_H
