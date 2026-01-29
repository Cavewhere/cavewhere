#include <catch2/catch_test_macros.hpp>

#include <QBuffer>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QUrl>

#include <memory>

#include <asyncfuture.h>

#include "ProjectFilenameTestHelper.h"
#include "cwCacheImageProvider.h"
#include "cwCropImageTask.h"
#include "cwDiskCacher.h"
#include "cwImageProvider.h"
#include "cwNoteLiDAR.h"
#include "cwNoteLiDARManager.h"
#include "cwProject.h"
#include "cwTrackedImage.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyNoteLiDARModel.h"

namespace {

struct ProjectFixture {
    QTemporaryDir rootDir;
    std::unique_ptr<cwProject> project;
    QString projectFile;
    QDir projectRootDir;
    QDir dataRootDir;
};

ProjectFixture makeProjectFixture()
{
    ProjectFixture fixture;
    REQUIRE(fixture.rootDir.isValid());

    fixture.project = std::make_unique<cwProject>();
    fixture.projectFile = fixture.project->filename();

    fixture.projectRootDir = QFileInfo(fixture.projectFile).absoluteDir();
    fixture.dataRootDir = ProjectFilenameTestHelper::projectDir(fixture.project.get());
    fixture.dataRootDir.mkpath(QStringLiteral("."));

    return fixture;
}

QString writeTestImage(const QDir& dir, const QString& fileName, const QColor& color)
{
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(color);
    QDir().mkpath(dir.absolutePath());
    const QString path = dir.filePath(fileName);
    REQUIRE(image.save(path));
    return path;
}

QByteArray pngBytes(const QImage& image)
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return data;
}

cwDiskCacher::Key lidarIconKey(const cwProject* project, const cwNoteLiDAR* note)
{
    cwDiskCacher::Key key;
    const QString absolutePath = project ? project->absolutePath(note, note->filename()) : QString();
    QFileInfo info(absolutePath);
    key.path = QDir(info.path());
    QString baseName = info.fileName();
    if (baseName.isEmpty()) {
        baseName = info.completeBaseName();
    }
    key.id = baseName + QStringLiteral(".icon.png");
    return key;
}

} // namespace

TEST_CASE("cwImageProvider caches under dataRoot", "[cwImageProvider][cache][dataRoot]") {
    auto fixture = makeProjectFixture();

    const QDir notesDir = QDir(fixture.dataRootDir.filePath("notes"));
    const QString imagePath = writeTestImage(notesDir, QStringLiteral("cache-source.png"), QColor(Qt::green));
    const QImage image(imagePath);

    cwDiskCacher::Key key = cwImageProvider::imageCacheKey(imagePath, QStringLiteral("scaled-16_16"), 1234);
    cwImageProvider::addToImageCache(fixture.dataRootDir.path(), image, key);

    const QString expectedPath = cwDiskCacher(fixture.dataRootDir).filePath(key);
    const QString wrongPath = cwDiskCacher(fixture.projectRootDir).filePath(key);

    CHECK(expectedPath != wrongPath);
    CHECK(QFileInfo::exists(expectedPath));
    CHECK(!QFileInfo::exists(wrongPath));
}

TEST_CASE("cwCropImageTask writes cache under dataRoot", "[cwCropImageTask][cache][dataRoot]") {
    auto fixture = makeProjectFixture();

    const QDir notesDir = QDir(fixture.dataRootDir.filePath("notes"));
    const QString imagePath = writeTestImage(notesDir, QStringLiteral("crop-source.png"), QColor(Qt::blue));

    cwImage original;
    original.setPath(imagePath);
    original.setOriginalSize(QSize(16, 16));

    cwCropImageTask task;
    task.setDataRootDir(fixture.dataRootDir);
    task.setOriginal(original);
    task.setRectF(QRectF(0.0, 0.0, 1.0, 1.0));

    auto future = task.crop();
    REQUIRE(AsyncFuture::waitForFinished(future, 2000));

    cwTrackedImagePtr cropped = future.result();
    REQUIRE(!cropped.isNull());

    const QString cachedPath = QFileInfo(cropped->path()).absoluteFilePath();
    CHECK(cachedPath.contains(QStringLiteral("/.cw_cache/")));
    CHECK(cachedPath.startsWith(fixture.dataRootDir.absolutePath() + QDir::separator()));
}

TEST_CASE("cwCacheImageProvider reads cached data from dataRoot", "[cwCacheImageProvider][cache][dataRoot]") {
    auto fixture = makeProjectFixture();

    const QDir notesDir = QDir(fixture.dataRootDir.filePath("notes"));
    const QString imagePath = writeTestImage(notesDir, QStringLiteral("provider-source.png"), QColor(Qt::red));

    cwDiskCacher::Key key = cwImageProvider::imageCacheKey(imagePath, QStringLiteral("icon"), 5678);
    cwDiskCacher cacher(fixture.dataRootDir);
    cacher.insert(key, pngBytes(QImage(imagePath)));

    cwCacheImageProvider provider;
    provider.setProjectDirectory(fixture.dataRootDir);

    const QString encodedKey = cwCacheImageProvider::encodeKey(key);
    const QString id = QString::fromLatin1(QUrl::toPercentEncoding(encodedKey));
    QSize size;
    QImage loaded = provider.requestImage(id, &size, QSize());

    REQUIRE(!loaded.isNull());
    REQUIRE(size == loaded.size());
}

TEST_CASE("cwNoteLiDARManager caches icons under dataRoot", "[cwNoteLiDARManager][cache][dataRoot]") {
    auto fixture = makeProjectFixture();

    auto* region = fixture.project->cavingRegion();
    region->setName(QStringLiteral("Region"));
    region->addCave();
    auto* cave = region->cave(0);
    cave->setName(QStringLiteral("Cave"));
    cave->addTrip();
    auto* trip = cave->trip(0);
    trip->setName(QStringLiteral("Trip"));

    fixture.project->waitSaveToFinish();

    cwNoteLiDAR note;
    note.setParentTrip(trip);
    note.setName(QStringLiteral("Scan"));
    note.setFilename(QStringLiteral("scan.glb"));

    cwNoteLiDARManager manager;
    manager.setProject(fixture.project.get());

    QImage icon(16, 16, QImage::Format_ARGB32);
    icon.fill(QColor(Qt::yellow));
    manager.saveIcon(icon, &note);

    const cwDiskCacher::Key key = lidarIconKey(fixture.project.get(), &note);
    const QString expectedPath = cwDiskCacher(fixture.dataRootDir).filePath(key);
    const QString wrongPath = cwDiskCacher(fixture.projectRootDir).filePath(key);

    CHECK(expectedPath != wrongPath);
    CHECK(QFileInfo::exists(expectedPath));
    CHECK(!QFileInfo::exists(wrongPath));
}
