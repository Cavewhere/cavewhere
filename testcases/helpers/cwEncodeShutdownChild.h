#ifndef CWENCODESHUTDOWNCHILD_H
#define CWENCODESHUTDOWNCHILD_H

//! Shared between cavewhere-encode-shutdown-child and the test that runs it
namespace cw::encodeShutdownChild {
    //! 2048 x 2048 makes 1371 chunks, so hundreds of waves are still queued at quit
    inline constexpr int kImageSize = 2048;

    //! How long the child waits for its encode to reach the task pool
    inline constexpr int kStartTimeoutMs = 30000;

    //! Printed once the encode is in flight, just before main returns
    inline constexpr char kInFlightMarker[] = "encode-in-flight\n";
}

#endif // CWENCODESHUTDOWNCHILD_H
