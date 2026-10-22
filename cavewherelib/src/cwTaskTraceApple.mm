//Our includes
#include "cwTaskTraceApple.h"

//Apple includes
#import <Metal/Metal.h>

std::optional<quint64> cwMetalAllocatedBytes()
{
    // The sampler's std::thread has no autorelease pool of its own
    @autoreleasepool {
        // Qt's RHI renders on the system default device; created once and kept for the process
        static id<MTLDevice> const device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            return std::nullopt;
        }
        return static_cast<quint64>(device.currentAllocatedSize);
    }
}
