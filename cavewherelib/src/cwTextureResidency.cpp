// Our includes
#include "cwTextureResidency.h"

// Qt includes
#include <QVector2D>
#include <QVector3D>

// Std includes
#include <algorithm>
#include <cmath>
#include <limits>

namespace {

    constexpr int kSmallestMipDimension = 1;
    constexpr int kMipDivisor = 2;
    constexpr int kBytesPerCompressedBlock = 16;
    constexpr int kBc7BlockDimension = 4;
    constexpr int kAstc4x4BlockDimension = 4;
    constexpr int kBytesPerRgbaPixel = 4;
    constexpr int kIndicesPerTriangle = 3;

    //Keeps pixelsPerMeter finite for geometry that straddles or sits behind the eye
    constexpr double kMinimumClipW = 1e-4;

    constexpr double kHalf = 0.5;

    //Clip space spans [-1, 1] vertically, so a projected meter covers
    //absP11 / kClipHeight of the viewport per unit w
    constexpr double kClipHeight = 2.0;

    constexpr int kBoxCornerCount = 8;

    int blockDimensionFor(QRhiTexture::Format format)
    {
        switch(format) {
        case QRhiTexture::BC7:
            return kBc7BlockDimension;
        case QRhiTexture::ASTC_4x4:
            return kAstc4x4BlockDimension;
        default:
            return 0;
        }
    }

    qint64 blocksAcross(int pixels, int blockDimension)
    {
        return (qint64(pixels) + blockDimension - 1) / blockDimension;
    }

    double triangleArea2D(const QVector2D& a, const QVector2D& b, const QVector2D& c)
    {
        const QVector2D edge1 = b - a;
        const QVector2D edge2 = c - a;
        return kHalf * std::abs(double(edge1.x()) * double(edge2.y())
                                - double(edge1.y()) * double(edge2.x()));
    }

    double triangleArea3D(const QVector3D& a, const QVector3D& b, const QVector3D& c)
    {
        return kHalf * double(QVector3D::crossProduct(b - a, c - a).length());
    }

    /**
     * The smallest positive clip-space w over the box's 8 corners, floored at
     * kMinimumClipW.
     */
    double nearestClipW(const QBox3D& bounds, const QMatrix4x4& viewProjection)
    {
        const QVector3D minimum = bounds.minimum();
        const QVector3D maximum = bounds.maximum();

        double nearest = std::numeric_limits<double>::max();
        for(int corner = 0; corner < kBoxCornerCount; corner++) {
            const QVector3D point((corner & 1) != 0 ? maximum.x() : minimum.x(),
                                  (corner & 2) != 0 ? maximum.y() : minimum.y(),
                                  (corner & 4) != 0 ? maximum.z() : minimum.z());
            const double w = double(viewProjection(3, 0)) * double(point.x())
                             + double(viewProjection(3, 1)) * double(point.y())
                             + double(viewProjection(3, 2)) * double(point.z())
                             + double(viewProjection(3, 3));
            if(w > 0.0) {
                nearest = std::min(nearest, w);
            }
        }

        if(nearest == std::numeric_limits<double>::max()) {
            return kMinimumClipW;
        }
        return std::max(nearest, kMinimumClipW);
    }
}

namespace cw::residency {

int mipLevelCount(QSize level0)
{
    if(level0.width() <= 0 || level0.height() <= 0) {
        return 0;
    }

    int levels = 1;
    QSize size = level0;
    while(size.width() > kSmallestMipDimension || size.height() > kSmallestMipDimension) {
        size = QSize(std::max(kSmallestMipDimension, size.width() / kMipDivisor),
                     std::max(kSmallestMipDimension, size.height() / kMipDivisor));
        levels++;
    }
    return levels;
}

QSize mipLevelSize(QSize level0, int level)
{
    if(level0.width() <= 0 || level0.height() <= 0) {
        return QSize();
    }

    QSize size = level0;
    for(int i = 0; i < level; i++) {
        size = QSize(std::max(kSmallestMipDimension, size.width() / kMipDivisor),
                     std::max(kSmallestMipDimension, size.height() / kMipDivisor));
    }
    return size;
}

qint64 mipLevelBytes(QRhiTexture::Format format, QSize levelSize)
{
    if(levelSize.width() <= 0 || levelSize.height() <= 0) {
        return 0;
    }

    const int blockDimension = blockDimensionFor(format);
    if(blockDimension > 0) {
        return blocksAcross(levelSize.width(), blockDimension)
               * blocksAcross(levelSize.height(), blockDimension)
               * kBytesPerCompressedBlock;
    }

    if(format == QRhiTexture::RGBA8) {
        return qint64(levelSize.width()) * qint64(levelSize.height()) * kBytesPerRgbaPixel;
    }

    return 0;
}

qint64 chainBytes(QRhiTexture::Format format, QSize level0, int topLevel)
{
    const int levels = mipLevelCount(level0);
    qint64 bytes = 0;
    for(int level = std::max(0, topLevel); level < levels; level++) {
        bytes += mipLevelBytes(format, mipLevelSize(level0, level));
    }
    return bytes;
}

int pinnedBaseLevel(QSize level0)
{
    const int levels = mipLevelCount(level0);
    for(int level = 0; level < levels; level++) {
        const QSize size = mipLevelSize(level0, level);
        if(std::max(size.width(), size.height()) <= kPinnedBaseMaxDimension) {
            return level;
        }
    }
    return std::max(0, levels - 1);
}

double uvPerMeter(const cwGeometry& geometry, const QMatrix4x4& modelMatrix, int maxSampleTriangles)
{
    const cwGeometry::VertexAttribute* position =
        geometry.attribute(cwGeometry::Semantic::Position);
    const cwGeometry::VertexAttribute* texCoord =
        geometry.attribute(cwGeometry::Semantic::TexCoord0);
    if(position == nullptr || texCoord == nullptr) {
        return 0.0;
    }

    const QVector<uint32_t>& indices = geometry.indices();
    const qsizetype triangleCount = indices.size() / kIndicesPerTriangle;
    const qsizetype vertexCount = geometry.vertexCount();
    if(triangleCount <= 0 || vertexCount <= 0 || maxSampleTriangles <= 0) {
        return 0.0;
    }

    const qsizetype sampleCount = std::min(triangleCount, qsizetype(maxSampleTriangles));

    double uvArea = 0.0;
    double worldArea = 0.0;
    for(qsizetype sample = 0; sample < sampleCount; sample++) {
        const qsizetype triangle = sample * triangleCount / sampleCount;

        QVector3D worldPoints[kIndicesPerTriangle];
        QVector2D uvPoints[kIndicesPerTriangle];
        bool validTriangle = true;
        for(int corner = 0; corner < kIndicesPerTriangle; corner++) {
            const qsizetype vertex =
                qsizetype(indices.at(triangle * kIndicesPerTriangle + corner));
            if(vertex < 0 || vertex >= vertexCount) {
                validTriangle = false;
                break;
            }
            worldPoints[corner] =
                modelMatrix.map(geometry.value<QVector3D>(position, vertex));
            uvPoints[corner] = geometry.value<QVector2D>(texCoord, vertex);
        }

        if(!validTriangle) {
            continue;
        }

        uvArea += triangleArea2D(uvPoints[0], uvPoints[1], uvPoints[2]);
        worldArea += triangleArea3D(worldPoints[0], worldPoints[1], worldPoints[2]);
    }

    if(uvArea <= 0.0 || worldArea <= 0.0) {
        return 0.0;
    }

    return std::sqrt(uvArea / worldArea);
}

int desiredTopLevel(const SelectionInput& input)
{
    const int baseLevel = pinnedBaseLevel(input.textureSize);
    if(baseLevel <= 0) {
        return 0;
    }

    if(input.uvPerMeter <= 0.0
       || input.modelScale <= 0.0
       || input.absP11 <= 0.0
       || input.viewportHeightPx <= 0
       || !input.worldBounds.isFinite()) {
        return 0;
    }

    const double texelsPerMeter =
        input.uvPerMeter
        * std::sqrt(double(input.textureSize.width()) * double(input.textureSize.height()))
        / input.modelScale;

    const double clipW = nearestClipW(input.worldBounds, input.viewProjection);
    const double pixelsPerMeter =
        input.absP11 * double(input.viewportHeightPx) / (kClipHeight * clipW);

    const double texelsPerPixel = texelsPerMeter / pixelsPerMeter;
    const double levels =
        std::floor(std::log2(std::max(1.0, texelsPerPixel * input.screenSpaceErrorPx)));

    return std::clamp(int(levels), 0, baseLevel);
}

QVector<Demotion> planEvictions(const QVector<ResidencyStats>& items, qint64 overshootBytes)
{
    QVector<Demotion> plan;
    if(overshootBytes <= 0) {
        return plan;
    }

    QVector<int> candidates;
    candidates.reserve(items.size());
    for(int i = 0; i < items.size(); i++) {
        const ResidencyStats& item = items.at(i);
        if(item.demotionInFlight || item.residentTopLevel < 0) {
            continue;
        }
        if(item.residentTopLevel >= pinnedBaseLevel(item.textureSize)) {
            continue;
        }
        candidates.append(i);
    }

    std::sort(candidates.begin(), candidates.end(), [&items](int left, int right) {
        const ResidencyStats& leftItem = items.at(left);
        const ResidencyStats& rightItem = items.at(right);
        if(leftItem.visibleThisFrame != rightItem.visibleThisFrame) {
            return rightItem.visibleThisFrame;
        }
        if(leftItem.lastVisibleFrame != rightItem.lastVisibleFrame) {
            return leftItem.lastVisibleFrame < rightItem.lastVisibleFrame;
        }
        return left < right;
    });

    qint64 reclaimed = 0;
    for(int index : candidates) {
        const ResidencyStats& item = items.at(index);
        const int baseLevel = pinnedBaseLevel(item.textureSize);
        const qint64 bytes = chainBytes(item.format, item.textureSize, item.residentTopLevel)
                             - chainBytes(item.format, item.textureSize, baseLevel);
        if(bytes <= 0) {
            continue;
        }

        plan.append({index, baseLevel, bytes});
        reclaimed += bytes;
        if(reclaimed >= overshootBytes) {
            break;
        }
    }

    return plan;
}

bool takeFromBudget(qint64& remainingBytes, qint64 cost, bool anythingUploadedThisFrame)
{
    if(cost <= remainingBytes || !anythingUploadedThisFrame) {
        remainingBytes -= cost;
        return true;
    }

    return false;
}

}
