#include "OffscreenRenderTester.h"
#include "LazFixtureHelper.h"
#include "cwRootData.h"

// Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTemporaryDir>

OffscreenRenderTester::OffscreenRenderTester(QObject* parent) :
    QObject(parent)
{
}

bool OffscreenRenderTester::addSyntheticPointCloud(QObject* rootData)
{
    auto* root = qobject_cast<cwRootData*>(rootData);
    if (!root) {
        return false;
    }

    QTemporaryDir dir;
    if (!dir.isValid()) {
        return false;
    }
    // addLazAndWait copies the file into the project, so the temp can vanish after.
    const QString lazPath = tempLazPath(dir, QStringLiteral("offscreen_diag"));
    writeMinimalLaz(lazPath);
    addLazAndWait(root, QStringList{lazPath});
    return true;
}

bool OffscreenRenderTester::windowHasRhi(QQuickItem* item) const
{
    if (!item || !item->window()) {
        return false;
    }
    QSGRendererInterface* renderer = item->window()->rendererInterface();
    if (!renderer) {
        return false;
    }
    return renderer->getResource(item->window(), QSGRendererInterface::RhiResource) != nullptr;
}

QString OffscreenRenderTester::tempPngPath(const QString& tag) const
{
    return QDir(QDir::tempPath()).filePath(
        QStringLiteral("cw_%1_%2.png").arg(tag).arg(QCoreApplication::applicationPid()));
}

bool OffscreenRenderTester::fileExists(const QString& path) const
{
    return QFile::exists(path);
}

bool OffscreenRenderTester::removeFile(const QString& path) const
{
    return QFile::exists(path) ? QFile::remove(path) : true;
}

QSize OffscreenRenderTester::imageSize(const QString& path) const
{
    return QImage(path).size();
}

bool OffscreenRenderTester::imageIsNonUniform(const QString& path) const
{
    const QImage image(path);
    if (image.isNull()) {
        return false;
    }

    const QRgb first = image.pixel(0, 0);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixel(x, y) != first) {
                return true;
            }
        }
    }
    return false;
}

double OffscreenRenderTester::nonBlackFraction(const QString& path) const
{
    const QImage image(path);
    if (image.isNull()) {
        return 0.0;
    }

    constexpr int kNearBlack = 16; // 0..255 per channel
    qsizetype nonBlack = 0;
    const qsizetype total = qsizetype(image.width()) * image.height();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb p = image.pixel(x, y);
            if (qRed(p) > kNearBlack || qGreen(p) > kNearBlack || qBlue(p) > kNearBlack) {
                ++nonBlack;
            }
        }
    }
    return total > 0 ? double(nonBlack) / double(total) : 0.0;
}
