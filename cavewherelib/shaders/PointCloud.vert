/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

// One octree node's points, quantized to uint16 per axis inside the node's
// cube (cw::octree::quantize). w is reserved.
layout(location = 0) in uvec4 qpos;

// Per instance, one per node: the node cube's min corner and the world size of
// one quantization step (nodeSize / cw::octree::kQuantMax). The whole node
// draws as a single instance, so every point of it dequantizes with these.
layout(location = 1) in vec4 nodeOriginScale;

// Per instance, one per node: the finest sample spacing, in meters, drawn
// anywhere inside this node's subtree. The cut is additive, so a node draws its
// own points over whatever its refined descendants draw, and this is the gap
// its sprites really have to close.
layout(location = 2) in float nodeFloor;

// Per instance, one per node, for the debug views alone: the frame the node
// became resident and its index in the manifest. Every node draws as instance
// 0 of its own draw, so gl_InstanceIndex cannot tell two nodes apart.
layout(location = 3) in uvec2 nodeIds;

layout(std140, binding = 0) uniform GlobalBlock {
    mat4 viewProjectionMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float devicePixelRatio;
    vec2 viewportSize;
};

// Per-cloud (binding 1): what every point of this cloud sizes itself from.
// spacingCoverage is the only knob a user turns (hold P + mouse wheel in the
// 3D view): the fraction of its own cell a sprite covers. sseThresholdPx is
// the projected spacing the octree cut refines to — the view's
// screenSpaceErrorPx times this cloud's current inflation — which is what lets
// each vertex work out the spacing around it from its own depth. The spacing
// really drawn is per instance instead (nodeFloor). rootSpacing is the sample
// spacing of this cloud's root, the coarsest level it has: the thinning below
// never aims for a gap wider than that, because past it there is no coarser
// level left to carry the surface. maxPointSizePx caps every sprite.
//
// The second row holds the knobs that switch the rules below
// (cw::pointcloud in cwPointCloudShading.h): thinningMode reshapes the weight,
// sizeMode picks what a sprite measures (fixedPointSizePx for FixedPixels),
// and floorMode drops the per-node floor. Their defaults are the rules as
// described here. The third row picks what the point is colored by
// (debugView, cw::pointcloud::DebugView) and carries the frame the residency
// age view counts from. The C++ PerCloudUniform matches the block field for
// field.
layout(std140, binding = 1) uniform PerCloudBlock {
    float spacingCoverage;
    float sseThresholdPx;
    float rootSpacing;
    float maxPointSizePx;
    int thinningMode;
    int sizeMode;
    int floorMode;
    float fixedPointSizePx;
    int debugView;
    uint currentFrame;
};

layout(location = 0) out vec4 vColor;

const int kThinningDiscrete = 1;
const int kThinningOff = 2;
const int kSizeFixedPixels = 1;
const int kSizeOwnSpacing = 2;
const int kFloorOff = 1;

const int kViewOff = 0;
const int kViewLevel = 1;
const int kViewNode = 2;
const int kViewRank = 3;
const int kViewWeight = 4;
const int kViewSpriteSize = 5;
const int kViewFloorRatio = 6;
const int kViewResidencyAge = 7;
const int kViewDecimationPreview = 8;

// cw::pointcloud::kLevelPalette, value for value
const int kLevelPaletteSize = 8;
const vec3 kLevelPalette[8] = vec3[8](
    vec3(0x6f, 0x8f, 0xf0) / 255.0,
    vec3(0x43, 0xbd, 0xe0) / 255.0,
    vec3(0x4e, 0xc9, 0x8a) / 255.0,
    vec3(0xbc, 0xd2, 0x3c) / 255.0,
    vec3(0xf2, 0xb7, 0x3a) / 255.0,
    vec3(0xf3, 0x80, 0x4f) / 255.0,
    vec3(0xb0, 0x7c, 0xf0) / 255.0,
    vec3(0xe0, 0x70, 0xc0) / 255.0);

const vec3 kWhite = vec3(1.0);
const vec4 kFlatColor = vec4(kWhite, 1.0);
// Every ramp stays off black, the clear color a lit-pixel count reads past
const float kRampFloor = 0.2;
const vec3 kZeroWeightColor = vec3(1.0, 0.15, 0.15);
const vec3 kFullWeightColor = vec3(0.15, 1.0, 0.15);
const vec3 kSmallSpriteColor = vec3(0.2, 0.3, 0.9);
const vec3 kLargeSpriteColor = vec3(1.0, 0.9, 0.2);
const vec3 kOversizedFloorColor = vec3(1.0, 0.2, 1.0);
// Octaves between a node's own spacing and its floor the ramp spans
const float kFloorRatioOctaves = 4.0;
const float kMinimumFloorSpacing = 1e-6;
const vec3 kSettledColor = vec3(0.4);
const float kResidencyFadeFrames = 60.0;
const vec3 kDecimatedColor = vec3(0.25);
const float kDecimatedSizePx = 1.0;
const uint kByteMask = 0xFFu;
const uint kGreenShift = 8u;
const uint kBlueShift = 16u;
const float kByteMax = 255.0;

// The continuous level of detail rule, value for value cw::clod
// (cwPointCloudClod.h) — change one and change the other, which is what the
// [PointCloudClod] and thinning cases check.
//
// A node's weight falls from 1 to 0 over the octave between its own sample
// spacing and twice it, and a point draws while its rank is under that weight,
// so a level fades out over the scales that lead to its departure instead of
// quartering the density the frame it leaves. The cut itself needs no change:
// a node refines while its children's weight is above zero somewhere, so the
// level that leaves the cut has already faded to nothing everywhere.
//
// The rank orders a node's points by the sub-octant of their parent cell, then
// by the sub-octant of their grandparent cell, then by a hash of the 27 bits
// below the cell — nine per axis, x lowest — so survivors stay spread over the
// parent grid and the thinning shows no structure under EDL. Ranks depend on
// the point alone, so the set drawn at one weight is a subset of the set drawn
// at any larger one and zooming never flickers a point off and back on.
const float kOctave = 2.0;
const float kQuantMax = 65535.0;
const float kSampleGridResolution = 128.0;
const int kOctantCount = 8;
const uint kQuantStepsPerCell = 512u;
const uint kSubCellMask = kQuantStepsPerCell - 1u;
const uint kSubCellBits = 9u;
const float kRankSteps = float(kOctantCount * kOctantCount);
const int kOctantOrder[8] = int[8](0, 4, 6, 2, 3, 7, 5, 1);

//The integer mix, and the bits of it the hash keeps
const uint kMixA = 0x27D4EB2Du;
const uint kMixB = 0x165667B1u;
const uint kShiftA = 15u;
const uint kShiftB = 13u;
const uint kKeptBits = 24u;
const uint kDropBits = 32u - kKeptBits;
const float kKeptScale = 1.0 / float(1u << kKeptBits);

uint mixBits(uint hash)
{
    hash *= kMixA;
    hash ^= hash >> kShiftA;
    hash *= kMixB;
    hash ^= hash >> kShiftB;
    return hash;
}

float subCellHash01(uvec3 quantized)
{
    uint hash = mixBits((quantized.x & kSubCellMask)
                        | ((quantized.y & kSubCellMask) << kSubCellBits)
                        | ((quantized.z & kSubCellMask) << (2u * kSubCellBits)));
    return float(hash >> kDropBits) * kKeptScale;
}

float pointRank(uvec3 quantized)
{
    uvec3 cell = quantized / kQuantStepsPerCell;
    int parentOctant = int((cell.x & 1u) | ((cell.y & 1u) << 1) | ((cell.z & 1u) << 2));
    int grandparentOctant = int(((cell.x >> 1) & 1u) | (((cell.y >> 1) & 1u) << 1)
                                | (((cell.z >> 1) & 1u) << 2));

    float steps = float(kOctantOrder[parentOctant] * kOctantCount
                        + kOctantOrder[grandparentOctant]);
    return (steps + subCellHash01(quantized)) / kRankSteps;
}

// What a drawn point is colored by under debugView. The level is the node's
// depth below the root, which the spacing ratio gives exactly: every level
// halves the spacing.
vec3 debugColor(float nodeSpacing, float rank, float weight, float sizePx)
{
    if (debugView == kViewLevel) {
        int level = int(round(log2(rootSpacing / nodeSpacing)));
        return kLevelPalette[level % kLevelPaletteSize];
    }
    if (debugView == kViewNode) {
        uint hash = mixBits(nodeIds.y);
        vec3 bytes = vec3(hash & kByteMask, (hash >> kGreenShift) & kByteMask,
                          (hash >> kBlueShift) & kByteMask) / kByteMax;
        return mix(vec3(kRampFloor), kWhite, bytes);
    }
    if (debugView == kViewRank) {
        return vec3(mix(kRampFloor, 1.0, rank));
    }
    if (debugView == kViewWeight) {
        return mix(kZeroWeightColor, kFullWeightColor, weight);
    }
    if (debugView == kViewSpriteSize) {
        float largest = max(log2(maxPointSizePx), 1.0);
        return mix(kSmallSpriteColor, kLargeSpriteColor, clamp(log2(sizePx) / largest, 0.0, 1.0));
    }
    if (debugView == kViewFloorRatio) {
        float octaves = log2(nodeSpacing / max(nodeFloor, kMinimumFloorSpacing));
        return mix(kWhite, kOversizedFloorColor, clamp(octaves / kFloorRatioOctaves, 0.0, 1.0));
    }
    if (debugView == kViewResidencyAge) {
        float age = float(currentFrame - nodeIds.x);
        return mix(kWhite, kSettledColor, clamp(age / kResidencyFadeFrames, 0.0, 1.0));
    }
    return kWhite;
}

// Keeps the sizing finite for points that straddle or sit behind the eye,
// matching cw::sse::kMinimumClipW so the shader and node selection floor w at
// the same place.
const float minimumClipW = 1e-4;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
};

void main(void)
{
    vec3 vVertex = nodeOriginScale.xyz + vec3(qpos.xyz) * nodeOriginScale.w;

    gl_Position = viewProjectionMatrix * vec4(vVertex, 1.0);

    // Perspective-correct screen size of a world-space length: a length of L
    // meters at clip-space depth w covers
    // L * projectionMatrix[1][1] * viewportSize.y * 0.5 / w pixels.
    // viewportSize is already the target's physical pixel size, so
    // devicePixelRatio stays out of this — a second factor drew sprites twice
    // as wide on a 2x display as selection (cw::sse::pixelsPerMeter) assumed.
    // abs() because projectionMatrix is pre-multiplied by Qt RHI's
    // clipSpaceCorrMatrix — [1][1] is negative on Metal/Vulkan/D3D and a
    // signed sizePx would always hit the lower clamp on those backends.
    float pixelsPerMeter = abs(projectionMatrix[1][1]) * viewportSize.y * 0.5
                         / max(gl_Position.w, minimumClipW);

    // The spacing the cut is aiming for right here, in meters, and how much of
    // this node's level still belongs at that spacing. A discarded point goes
    // outside the clip volume, the cheapest discard a vertex shader has.
    // cw::clod::targetSpacing is this line, root floor included.
    float targetSpacing = min(sseThresholdPx / pixelsPerMeter, rootSpacing);
    float nodeSpacing = nodeOriginScale.w * kQuantMax / kSampleGridResolution;
    float weight = clamp(log2(kOctave * nodeSpacing / targetSpacing), 0.0, 1.0);
    if (thinningMode == kThinningDiscrete) {
        // Compared directly rather than through log2, so the step lands on the
        // same side as cw::clod's at the octave boundary on every backend.
        weight = nodeSpacing >= targetSpacing ? 1.0 : 0.0;
    } else if (thinningMode == kThinningOff) {
        weight = 1.0;
    }

    float rank = pointRank(qpos.xyz);
    if (rank >= weight) {
        if (debugView == kViewDecimationPreview) {
            // Left where it is, so what the thinning took shows in place
            vColor = vec4(kDecimatedColor, 1.0);
            gl_PointSize = kDecimatedSizePx;
            return;
        }
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        gl_PointSize = 1.0;
        vColor = kFlatColor;
        return;
    }

    // Sized against the spacing the cut is aiming for right here: the octree
    // refines a node while its sample spacing projects wider than
    // sseThresholdPx, so the points around this vertex sit about
    // sseThresholdPx / pixelsPerMeter meters apart — finer close to the eye,
    // coarser far from it, exactly how the drawn level steps out with distance
    // in perspective. Covering spacingCoverage of that gap is therefore a
    // constant pixel size, which is what lets a far tile drawn at a coarse
    // level cover its own cell as well as a near tile covers its finer one.
    // Coverage is the whole rule: it is the one number that predicts whether
    // the surface reads solid or shows holes, so there is no separate tuned
    // world radius competing with it.
    //
    // The threshold term describes the spacing a cut that keeps up has on
    // screen. Where the point budget or a node still streaming holds this part
    // of the cloud coarser than that, nodeFloor is the wider gap that is
    // really there, so coverage of it becomes a world-space floor. It is the
    // finest spacing drawn under this node rather than this node's own, so a
    // refined region's ancestors keep the size of the region they sit in
    // instead of blowing up to their own coarse spacing.
    float floorSpacing = sizeMode == kSizeOwnSpacing ? nodeSpacing : nodeFloor;
    float floorPx = floorMode == kFloorOff ? 0.0
                                           : spacingCoverage * floorSpacing * pixelsPerMeter;
    float sizePx = sizeMode == kSizeFixedPixels
                       ? fixedPointSizePx
                       : max(floorPx, spacingCoverage * sseThresholdPx);
    // 1px floor: zoomed-out clouds were getting darker than zoomed-in ones
    // because a large min clamp made far points overdraw and stack EDL
    // darkening. Letting size shrink to a single pixel keeps overdraw flat
    // across zoom levels. EDL response gets noisier on lone 1px sprites but
    // that's preferable to the inconsistent intensity.
    gl_PointSize = clamp(sizePx, 1.0, maxPointSizePx);

    vColor = debugView == kViewOff ? kFlatColor
                            : vec4(debugColor(nodeSpacing, rank, weight, gl_PointSize), 1.0);
}
