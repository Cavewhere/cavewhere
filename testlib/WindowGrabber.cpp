#include "WindowGrabber.h"

// Qt includes
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPoint>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRect>
#include <QRectF>

namespace {
// Environment variable the gen-manual-screenshots.sh script sets to redirect the
// output at docs/manual/images/; unset in a plain test run so nothing lands in
// the source tree.
constexpr const char* kOutputDirEnv = "CW_MANUAL_IMAGE_DIR";
constexpr auto kPngSuffix = ".png";
}

WindowGrabber::WindowGrabber(QObject* parent)
    : QObject(parent)
{
}

QString WindowGrabber::outputDir() const
{
    QString dir = qEnvironmentVariable(kOutputDirEnv);
    if (dir.isEmpty()) {
        dir = QDir(QDir::tempPath()).filePath(
            QStringLiteral("cw_manual_images_%1").arg(QCoreApplication::applicationPid()));
    }
    QDir().mkpath(dir);
    return dir;
}

QString WindowGrabber::resolvePath(const QString& name) const
{
    const QString fileName = name.endsWith(QLatin1String(kPngSuffix))
                                 ? name
                                 : name + QLatin1String(kPngSuffix);
    return QDir(outputDir()).filePath(fileName);
}

QImage WindowGrabber::grabWindow(QQuickItem* item) const
{
    if (!item || !item->window()) {
        qWarning() << "WindowGrabber: item has no window";
        return QImage();
    }
    QImage image = item->window()->grabWindow();
    if (image.isNull()) {
        qWarning() << "WindowGrabber: grabWindow() returned a null image "
                      "(no GPU-backed QPA?)";
    }
    return image;
}

QString WindowGrabber::grabToFile(QQuickItem* anyItem, const QString& name)
{
    const QImage image = grabWindow(anyItem);
    if (image.isNull()) {
        return QString();
    }
    const QString path = resolvePath(name);
    if (!image.save(path)) {
        qWarning() << "WindowGrabber: failed to write" << path;
        return QString();
    }
    return path;
}

QString WindowGrabber::grabItemToFile(QQuickItem* item, const QString& name, int margin)
{
    const QImage image = grabWindow(item);
    if (image.isNull() || !item) {
        return QString();
    }

    // grabWindow() returns physical pixels tagged with the window's DPR; the
    // item rect is in logical scene coordinates, so scale by the DPR and clamp to
    // the grabbed image so the crop never runs off an edge.
    const qreal dpr = image.devicePixelRatio();
    const QRectF sceneRect = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
    const QRectF padded = sceneRect.adjusted(-margin, -margin, margin, margin);

    QRect pixelRect(qRound(padded.x() * dpr), qRound(padded.y() * dpr),
                    qRound(padded.width() * dpr), qRound(padded.height() * dpr));
    pixelRect = pixelRect.intersected(QRect(QPoint(0, 0), image.size()));
    if (pixelRect.isEmpty()) {
        qWarning() << "WindowGrabber: item crop rect is empty";
        return QString();
    }

    QImage cropped = image.copy(pixelRect);
    cropped.setDevicePixelRatio(dpr);
    const QString path = resolvePath(name);
    if (!cropped.save(path)) {
        qWarning() << "WindowGrabber: failed to write" << path;
        return QString();
    }
    return path;
}

bool WindowGrabber::fileExists(const QString& path) const
{
    return QFile::exists(path);
}

QSize WindowGrabber::imageSize(const QString& path) const
{
    return QImage(path).size();
}

bool WindowGrabber::removeFile(const QString& path) const
{
    return QFile::exists(path) ? QFile::remove(path) : true;
}
