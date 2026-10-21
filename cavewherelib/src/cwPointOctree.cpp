// cwPointOctree.cpp
#include "cwPointOctree.h"
#include "cwPointOctreeManifest.h"
#include "cwDiskCacher.h"
#include "cwImageProvider.h"

//Qt includes
#include <QFile>
#include <QFileInfo>
#include <QtEndian>

//Std includes
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace {
    //The uint16 x, y, z, reserved of one on-disk point
    constexpr int kAxesPerPoint = 4;

    //The steps between the first and last native spacing codes, 1 and kQuantMax
    constexpr double kLocalSpacingCodeSteps = double(cw::octree::kQuantMax - 1);

    quint16 quantizeAxis(double value, double minimum, double maximum)
    {
        const double size = maximum - minimum;
        if(size <= 0.0) {
            return 0;
        }

        const double scaled = std::round((value - minimum) / size * cw::octree::kQuantMax);
        return static_cast<quint16>(std::clamp(scaled, 0.0, static_cast<double>(cw::octree::kQuantMax)));
    }

    float dequantizeAxis(quint16 value, double minimum, double maximum)
    {
        const double size = maximum - minimum;
        return static_cast<float>(minimum + size * value / cw::octree::kQuantMax);
    }

    //! Spreads the low cw::octree::kMortonBitsPerAxis bits of @a value so bit i
    //! lands at bit 3i, leaving the other two thirds of the key for the other
    //! axes.
    /*!
        Five doublings of the gap between bits, each masking off the copies that
        landed where they do not belong. These are the standard Morton spread
        constants for a 21 bit axis, which covers our 16, and the whole thing
        unrolls to about a dozen instructions — the build quantizes hundreds of
        millions of points, so a per-bit loop here shows up in the wall clock.
    */
    quint64 spreadForMorton(quint16 value)
    {
        constexpr std::array<int, 5> kSpreadShifts {32, 16, 8, 4, 2};
        constexpr std::array<quint64, 5> kSpreadMasks {
            0x001f00000000ffffULL,
            0x001f0000ff0000ffULL,
            0x100f00f00f00f00fULL,
            0x10c30c30c30c30c3ULL,
            0x1249249249249249ULL
        };
        static_assert(cw::octree::kMortonBitsPerAxis <= 21,
                      "The spread constants below carry 21 bits per axis.");

        quint64 spread = value;
        for(size_t step = 0; step < kSpreadShifts.size(); step++) {
            spread = (spread | (spread << kSpreadShifts[step])) & kSpreadMasks[step];
        }
        return spread;
    }

    struct KeyedPoint {
        quint64 key = 0;
        cw::octree::QuantizedPoint point;
    };
}

namespace cw::octree {

QuantizedPoint quantize(const QVector3D& point, const QBox3D& nodeBounds)
{
    const QVector3D minimum = nodeBounds.minimum();
    const QVector3D maximum = nodeBounds.maximum();

    QuantizedPoint quantized;
    quantized.x = quantizeAxis(point.x(), minimum.x(), maximum.x());
    quantized.y = quantizeAxis(point.y(), minimum.y(), maximum.y());
    quantized.z = quantizeAxis(point.z(), minimum.z(), maximum.z());
    return quantized;
}

QVector3D dequantize(const QuantizedPoint& point, const QBox3D& nodeBounds)
{
    const QVector3D minimum = nodeBounds.minimum();
    const QVector3D maximum = nodeBounds.maximum();

    return QVector3D(dequantizeAxis(point.x, minimum.x(), maximum.x()),
                     dequantizeAxis(point.y, minimum.y(), maximum.y()),
                     dequantizeAxis(point.z, minimum.z(), maximum.z()));
}

quint64 mortonKey(const QuantizedPoint& point)
{
    return spreadForMorton(point.x)
           | (spreadForMorton(point.y) << 1)
           | (spreadForMorton(point.z) << 2);
}

QByteArray quantizeAll(const QVector<QVector3D>& points, const QBox3D& nodeBounds)
{
    return quantizeAll(points, QVector<quint16>(), nodeBounds);
}

quint16 encodeLocalSpacing(double spacing)
{
    //std::clamp keeps a NaN, so the lower clamp is written to send it to the floor
    const double clamped = spacing >= kMinLocalSpacingUnits
                               ? std::min(spacing, kMaxLocalSpacingUnits)
                               : kMinLocalSpacingUnits;
    const double position = std::log2(clamped / kMinLocalSpacingUnits) / kLocalSpacingCodeOctaves;
    return quint16(1 + std::lround(position * kLocalSpacingCodeSteps));
}

double decodeLocalSpacing(quint16 code, double gridSpacing)
{
    if(code == kUnmeasuredLocalSpacingCode) {
        return gridSpacing;
    }
    return kMinLocalSpacingUnits
           * std::exp2(double(code - 1) / kLocalSpacingCodeSteps * kLocalSpacingCodeOctaves);
}

QByteArray quantizeAll(const QVector<QVector3D>& points,
                       const QVector<quint16>& localSpacingCodes,
                       const QBox3D& nodeBounds)
{
    Q_ASSERT(localSpacingCodes.isEmpty() || localSpacingCodes.size() == points.size());
    const bool hasCodes = localSpacingCodes.size() == points.size();

    //The key travels with its point so the sort compares a field rather than
    //re-deriving the interleave on every comparison; the reserved word rides
    //in the point, so a code stays with its point through the sort
    QVector<KeyedPoint> quantized;
    quantized.reserve(points.size());
    for(qsizetype i = 0; i < points.size(); i++) {
        QuantizedPoint value = quantize(points.at(i), nodeBounds);
        value.reserved = hasCodes ? localSpacingCodes.at(i) : kUnmeasuredLocalSpacingCode;
        quantized.append(KeyedPoint {mortonKey(value), value});
    }

    //Stable, so points that share a cell keep their input order and the payload
    //is the same bytes every build
    std::stable_sort(quantized.begin(), quantized.end(),
                     [](const KeyedPoint& left, const KeyedPoint& right) {
                         return left.key < right.key;
                     });

    QByteArray bytes(points.size() * kBytesPerPoint, Qt::Uninitialized);

    char* writePoint = bytes.data();
    for(const KeyedPoint& keyed : std::as_const(quantized)) {
        const QuantizedPoint& point = keyed.point;
        const quint16 axes[kAxesPerPoint] = {point.x, point.y, point.z, point.reserved};
        qToLittleEndian<quint16>(axes, kAxesPerPoint, writePoint);
        writePoint += kBytesPerPoint;
    }

    return bytes;
}

QString nodeName(const QVector<int>& octantPath)
{
    QString name = QStringLiteral("r");
    for(int octant : octantPath) {
        name += QString::number(octant);
    }
    return name;
}

QString sourceFingerprint(const QString& lazPath, const QString& sourceCS, const QString& frameCS)
{
    QFile file(lazPath);
    if(!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    const qint64 fileSize = file.size();
    const quint64 littleEndianSize = qToLittleEndian(static_cast<quint64>(fileSize));

    QByteArray hashed(reinterpret_cast<const char*>(&littleEndianSize), sizeof(littleEndianSize));

    //A short read means the file changed or the device failed, so the fingerprint is unusable
    const auto readExactly = [&file, &hashed](qint64 count) {
        const QByteArray window = file.read(count);
        hashed += window;
        return window.size() == count;
    };

    if(fileSize <= 2 * kFingerprintWindowBytes) {
        if(!readExactly(fileSize)) {
            return QString();
        }
    } else {
        if(!readExactly(kFingerprintWindowBytes)) {
            return QString();
        }

        if(!file.seek(fileSize - kFingerprintWindowBytes)) {
            return QString();
        }

        if(!readExactly(kFingerprintWindowBytes)) {
            return QString();
        }
    }

    hashed += sourceCS.toUtf8();
    hashed += frameCS.toUtf8();

    return QString::number(cwImageProvider::toHash(hashed), 16);
}

quint64 cellKey(const cwPointOctreeNode& node)
{
    return cellKey(node.level, node.x, node.y, node.z);
}

QString packPath(const QString& cacheRootPath, const QString& lazPath)
{
    const QFileInfo info(lazPath);
    const QString id = info.fileName()
                       + QStringLiteral("-octree")
                       + QString::number(kFormatGeneration)
                       + QStringLiteral(".cwpack");
    return cwDiskCacher(cacheRootPath).filePath(cwDiskCacher::Key {id, info.path(), QString()});
}

}
