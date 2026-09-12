// cwPointOctree.cpp
#include "cwPointOctree.h"
#include "cwImageProvider.h"

//Qt includes
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtEndian>

//Std includes
#include <algorithm>
#include <cmath>

namespace {
    //The uint16 x, y, z, reserved of one on-disk point
    constexpr int kAxesPerPoint = 4;

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

    cwDiskCacher::Key cacheKey(const QString& lazPath, const QString& fingerprint, const QString& suffix)
    {
        const QFileInfo info(lazPath);
        return cwDiskCacher::Key {
            info.fileName()
                + QStringLiteral("-octree")
                + QString::number(cw::octree::kFormatGeneration)
                + QStringLiteral("-")
                + suffix,
            info.dir(),
            fingerprint
        };
    }
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

QByteArray quantizeAll(const QVector<QVector3D>& points, const QBox3D& nodeBounds)
{
    QByteArray bytes(points.size() * kBytesPerPoint, Qt::Uninitialized);

    char* writePoint = bytes.data();
    for(const QVector3D& point : points) {
        const QuantizedPoint quantized = quantize(point, nodeBounds);
        const quint16 axes[kAxesPerPoint] = {quantized.x, quantized.y, quantized.z, quantized.reserved};
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

cwDiskCacher::Key manifestKey(const QString& lazPath, const QString& fingerprint)
{
    return cacheKey(lazPath, fingerprint, QStringLiteral("manifest"));
}

cwDiskCacher::Key nodeKey(const QString& lazPath, const QString& fingerprint, const QString& nodeName)
{
    return cacheKey(lazPath, fingerprint, nodeName);
}

}
