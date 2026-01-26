#include <catch2/catch_test_macros.hpp>

// Qt
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSize>
#include <QUrl>

// SUT + deps
#include "cwSurveyNotesConcatModel.h"
#include "cwSurveyNoteModel.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwImageProvider.h"
#include "cwTrip.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwProject.h"
#include "cwRootData.h"

//Our includes
#include "LoadProjectHelper.h"

// If needed: #include "cwFutureManagerModel.h" (or wherever waitForFinished() lives)
// If PDF converter is not available in CI, we’ll only use JPG + GLB in this test.

static QUrl urlFromPath(const QString& absolutePath) {
    return QUrl::fromLocalFile(absolutePath);
}

// static QString makeTinyJpeg(const QDir& dir, const QString& basename) {
//     QImage img(2, 2, QImage::Format_RGBA8888);
//     img.fill(Qt::white);
//     const QString path = dir.filePath(basename);
//     REQUIRE(img.save(path, "JPG"));
//     return path;
// }

// static QString makeDummyFile(const QDir& dir, const QString& basename) {
//     const QString path = dir.filePath(basename);
//     QFile f(path);
//     REQUIRE(f.open(QIODevice::WriteOnly));
//     f.write("x"); // just to ensure the file exists
//     f.close();
//     return path;
// }

// // Helper: wait until rowCount reaches target or timeout
// static bool waitForRowCount(QAbstractItemModel* model, int expected, int timeoutMs = 5000) {
//     QElapsedTimer timer;
//     timer.start();

//     // Use rowsInserted spy for faster wakeups
//     QSignalSpy spy(model, &QAbstractItemModel::rowsInserted);

//     while (timer.elapsed() < timeoutMs) {
//         if (model->rowCount() >= expected) {
//             return true;
//         }
//         // If we already saw rowsInserted, process them
//         if (!spy.isEmpty() && model->rowCount() >= expected) {
//             return true;
//         }
//         // Spin the event loop briefly
//         QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
//         spy.wait(50);
//     }
//     return model->rowCount() >= expected;
// }

TEST_CASE("cwSurveyNotesConcatModel integrates with real cwTrip models", "[cwSurveyNotesConcatModel]") {
    // --- Arrange a Project → Region → Cave → Trip tree so models resolve project() ---

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    //Add the cave
    cwCave* cave = new cwCave();
    cave->setName("test");

    //Add a trip
    cwTrip* trip = new cwTrip();
    trip->setName("test trip");
    cave->addTrip(trip);

    //Add them all
    project->cavingRegion()->addCave(cave);

    // (Sanity) The trip should expose non-null models
    REQUIRE(trip->notes()      != nullptr);
    REQUIRE(trip->notesLiDAR() != nullptr);

    // If your project must be “opened/initialized” to a folder, do that:
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir baseDir(tempDir.path());

    // TODO: If cwProject needs an explicit directory setter/open, call it here, e.g.:
    // project->setProjectDirectory(baseDir);  // adjust to your real API
    // REQUIRE(project->directory() == baseDir); // if applicable

    // --- System under test ---
    cwSurveyNotesConcatModel concat;
    concat.setTrip(trip);

    // Starts empty
    REQUIRE(concat.rowCount() == 0);

    // --- Create one small real JPEG and one dummy GLB file ---
    const QString jpgPath = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    const QString glbPath = copyToTempFolder("://datasets/test_cwSurveyNotesConcatModel/bones.glb");

    const QList<QUrl> files = {
        urlFromPath(jpgPath),
        urlFromPath(glbPath)
    };

    // --- Act: route via concatenated model ---
    concat.addFiles(files);

    // If your addImages/addFiles are async, you might have a central future manager.
    // Prefer your project’s wait method if available to avoid flakiness:
    rootData->futureManagerModel()->waitForFinished();

    // --- Assert: combined rows == 2 ---
    REQUIRE(concat.rowCount() == 2);

    // Validate routing using PathRole (models normalize differently; we just check suffixes)
    const QModelIndex i0 = concat.index(0, 0);
    const QModelIndex i1 = concat.index(1, 0);
    REQUIRE(i0.isValid());
    REQUIRE(i1.isValid());

    const QString p0 = concat.data(i0, cwSurveyNoteModelBase::PathRole).toString();
    const QString p1 = concat.data(i1, cwSurveyNoteModelBase::PathRole).toString();

    const bool hasJpg = p0.endsWith(".PNG", Qt::CaseInsensitive) || p1.endsWith(".PNG", Qt::CaseInsensitive);
    const bool hasGlb = p0.endsWith(".glb", Qt::CaseInsensitive) || p1.endsWith(".glb", Qt::CaseInsensitive);
    CHECK(hasJpg);
    CHECK(hasGlb);
}

TEST_CASE("cwSurveyNotesConcatModel addFiles uses absolute paths with cwImageProvider", "[cwSurveyNotesConcatModel]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    cwCave* cave = new cwCave();
    cave->setName("test");

    cwTrip* trip = new cwTrip();
    trip->setName("test trip");
    cave->addTrip(trip);
    project->cavingRegion()->addCave(cave);

    cwSurveyNotesConcatModel concat;
    concat.setTrip(trip);

    const QString pngPath = copyToTempFolder("://datasets/test_cwNote/testpage.png");
    REQUIRE(!pngPath.isEmpty());
    const QList<QUrl> files = { QUrl::fromLocalFile(pngPath) };

    concat.addFiles(files);
    rootData->futureManagerModel()->waitForFinished();

    REQUIRE(concat.rowCount() == 1);
    const QModelIndex index = concat.index(0, 0);
    REQUIRE(index.isValid());

    const QString imageUrlString = concat.data(index, cwSurveyNoteModelBase::PathRole).toString();
    REQUIRE(imageUrlString.startsWith(QStringLiteral("image://")));

    const QUrl imageUrl(imageUrlString);
    REQUIRE(imageUrl.isValid());
    CHECK(imageUrl.scheme() == QStringLiteral("image"));
    CHECK(imageUrl.host() == cwImageProvider::name());

    QString providerPath = imageUrl.path(QUrl::FullyDecoded);
    if (providerPath.size() >= 3 && providerPath[0] == QLatin1Char('/') && providerPath[2] == QLatin1Char(':')) {
        //Strip the leading "/" on windows                                                                                                                                                                                                                                                             }
        providerPath.remove(0, 1);
    }

    INFO("Url:" << imageUrl.toString().toStdString());
    INFO("ProviderPath:\"" << imageUrl.path().toStdString() << "\"");
    REQUIRE(QFileInfo(providerPath).isAbsolute());
    REQUIRE(QFileInfo(providerPath).exists());

    cwImageProvider provider;
    provider.setProjectPath(project->filename());

    QSize size;
    const QImage image = provider.requestImage(providerPath, &size, QSize(64, 64));
    REQUIRE(!image.isNull());
    REQUIRE(size.isValid());
}

TEST_CASE("cwSurveyNotesConcatModel allows duplicate image files", "[cwSurveyNotesConcatModel]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    cwCave* cave = new cwCave();
    cave->setName("test");

    cwTrip* trip = new cwTrip();
    trip->setName("test trip");
    cave->addTrip(trip);
    project->cavingRegion()->addCave(cave);

    cwSurveyNotesConcatModel concat;
    concat.setTrip(trip);

    const QString pngPath = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(!pngPath.isEmpty());

    const QList<QUrl> files = {
        QUrl::fromLocalFile(pngPath),
        QUrl::fromLocalFile(pngPath)
    };

    concat.addFiles(files);
    rootData->futureManagerModel()->waitForFinished();

    REQUIRE(concat.rowCount() == 2);

    const QModelIndex i0 = concat.index(0, 0);
    const QModelIndex i1 = concat.index(1, 0);
    REQUIRE(i0.isValid());
    REQUIRE(i1.isValid());

    const QString p0 = concat.data(i0, cwSurveyNoteModelBase::PathRole).toString();
    const QString p1 = concat.data(i1, cwSurveyNoteModelBase::PathRole).toString();

    REQUIRE(p0 != p1);
}
