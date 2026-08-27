// test_cwRhiLimits.cpp
// Catch2 unit tests for the QRhiBuffer size clamp helpers in cwRhiLimits.h.

#include <catch2/catch_test_macros.hpp>

#include "cwRhiLimits.h"

namespace {
constexpr int kInterleavedPositionStride = 12; // 3 floats
constexpr int kInterleavedPositionColorStride = 20; // 3 floats + 2 shorts + padding
}

TEST_CASE("cwRhiLimits: sizes at or below the limit pass through", "[RhiLimits]") {
    CHECK(cw::clampedVertexBytes(0, kInterleavedPositionStride) == 0);
    CHECK(cw::clampedVertexBytes(1200, kInterleavedPositionStride) == 1200);
    CHECK(cw::clampedVertexBytes(cw::kMaxRhiBufferBytes, kInterleavedPositionStride)
          == cw::kMaxRhiBufferBytes);
}

TEST_CASE("cwRhiLimits: oversized buffers clamp to a whole-vertex multiple", "[RhiLimits]") {
    const qint64 oversized = cw::kMaxRhiBufferBytes * 3;

    for (int stride : {kInterleavedPositionStride, kInterleavedPositionColorStride}) {
        const qint64 clamped = cw::clampedVertexBytes(oversized, stride);
        CHECK(clamped <= cw::kMaxRhiBufferBytes);
        CHECK(clamped % stride == 0);
        CHECK(clamped > cw::kMaxRhiBufferBytes - stride);
    }
}

TEST_CASE("cwRhiLimits: the smallest oversized buffer still clamps", "[RhiLimits]") {
    const qint64 justOver = cw::kMaxRhiBufferBytes + 1;
    const qint64 clamped = cw::clampedVertexBytes(justOver, kInterleavedPositionStride);

    CHECK(clamped < justOver);
    CHECK(clamped % kInterleavedPositionStride == 0);
}

TEST_CASE("cwRhiLimits: vertex count matches the clamped byte range", "[RhiLimits]") {
    CHECK(cw::clampedVertexCount(1200, kInterleavedPositionStride) == 100);

    const qint64 oversized = cw::kMaxRhiBufferBytes * 2;
    const qint64 count = cw::clampedVertexCount(oversized, kInterleavedPositionColorStride);
    CHECK(count * kInterleavedPositionColorStride
          == cw::clampedVertexBytes(oversized, kInterleavedPositionColorStride));
}

TEST_CASE("cwRhiLimits: a zero stride clamps to the raw buffer limit", "[RhiLimits]") {
    CHECK(cw::clampedVertexBytes(cw::kMaxRhiBufferBytes * 2, 0) == cw::kMaxRhiBufferBytes);
    CHECK(cw::clampedVertexCount(cw::kMaxRhiBufferBytes * 2, 0) == 0);
}
