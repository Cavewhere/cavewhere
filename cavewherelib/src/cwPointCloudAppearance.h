/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPOINTCLOUDAPPEARANCE_H
#define CWPOINTCLOUDAPPEARANCE_H

// Std includes
#include <optional>

// Point-cloud appearance override, packed into a cwAppearanceOverride for an
// offscreen render job. The point cloud owns this schema because its fields are
// point-cloud-specific (a line plot has no world radius); the generic job API
// carries it only as an opaque cwAppearanceOverride. lidarPrivate extends this
// struct (renderer-internal: monochrome, clip prism) without touching the shared
// API — the same per-branch divergence the PerCloudUniform layout already has.
struct cwPointCloudAppearance {
    // Absent = render at the cloud's live world radius (slot 0). Present =
    // override the world-space sprite radius for this job only.
    std::optional<float> worldRadius;
};

#endif // CWPOINTCLOUDAPPEARANCE_H
