#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

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
#include "cwNote.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwProject.h"
#include "cwRootData.h"

//Our includes
#include "LoadProjectHelper.h"



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
    const QString jpgPath = copyToTempFolder(testcasesDatasetPath("test_cwTextureUploadTask/PhakeCave.PNG"));
    const QString glbPath = copyToTempFolder(testcasesDatasetPath("test_cwSurveyNotesConcatModel/bones.glb"));

    const QList<QUrl> files = {
        QUrl::fromLocalFile(jpgPath),
        QUrl::fromLocalFile(glbPath)
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
    struct Names {
        QString cave;
        QString trip;
    };
    const Names names = GENERATE(Names{QStringLiteral("test"), QStringLiteral("test trip")},
                                 Names{QStringLiteral("Cave #2"), QStringLiteral("Trip #3")},
                                 Names{QStringLiteral("100% cave"), QStringLiteral("50% trip")},
                                 Names{QStringLiteral("Cave a%23b"), QStringLiteral("Trip a%23b")},
                                 Names{QStringLiteral("Cave #a%23b%"), QStringLiteral("Trip #%20%")});
    INFO("Cave:" << names.cave.toStdString() << " Trip:" << names.trip.toStdString());

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    cwCave* cave = new cwCave();
    cave->setName(names.cave);

    cwTrip* trip = new cwTrip();
    trip->setName(names.trip);
    cave->addTrip(trip);
    project->cavingRegion()->addCave(cave);

    cwSurveyNotesConcatModel concat;
    concat.setTrip(trip);

    const QString pngPath = copyToTempFolder(testcasesDatasetPath("test_cwNote/testpage.png"));
    REQUIRE(!pngPath.isEmpty());
    const QList<QUrl> files = { QUrl::fromLocalFile(pngPath) };

    concat.addFiles(files);
    rootData->futureManagerModel()->waitForFinished();

    REQUIRE(concat.rowCount() == 1);
    const QModelIndex index = concat.index(0, 0);
    REQUIRE(index.isValid());

    //sanitizeFileName keeps # and % today, so the cave and trip folders on disk carry the
    //names as written. This flags the day sanitizing changes.
    REQUIRE(trip->notes()->notes().size() == 1);
    const cwNote* note = trip->notes()->notes().first();
    const QString noteAbsolutePath = project->absolutePath(note);
    const QString expectedSegments = QStringLiteral("/%1/trips/%2/notes/").arg(names.cave, names.trip);
    INFO("NoteAbsolutePath:\"" << noteAbsolutePath.toStdString() << "\"");
    CHECK(noteAbsolutePath.contains(expectedSegments));
    CHECK(QFileInfo::exists(noteAbsolutePath));

    const QString imageUrlString = concat.data(index, cwSurveyNoteModelBase::PathRole).toString();
    REQUIRE(imageUrlString.startsWith(QStringLiteral("image://")));

    const QUrl imageUrl(imageUrlString);
    REQUIRE(imageUrl.isValid());
    CHECK(imageUrl.scheme() == QStringLiteral("image"));
    CHECK(imageUrl.host() == cwImageProvider::name());

    const QString requestId = imageProviderRequestId(imageUrlString);
    const QString providerPath = QUrl::fromPercentEncoding(requestId.toUtf8());

    INFO("Url:" << imageUrlString.toStdString());
    INFO("ProviderPath:\"" << providerPath.toStdString() << "\"");
    INFO("RequestId:\"" << requestId.toStdString() << "\"");
    REQUIRE(QFileInfo(providerPath).isAbsolute());
    REQUIRE(QFileInfo(providerPath).exists());
    CHECK(providerPath.toStdString() == noteAbsolutePath.toStdString());

    cwImageProvider provider;
    provider.setDataRootDir(project->dataRootDir());

    QSize size;
    const QImage image = provider.requestImage(requestId, &size, QSize(64, 64));
    REQUIRE(!image.isNull());
    REQUIRE(size.isValid());
}

TEST_CASE("cwSurveyNotesConcatModel switching trips updates rows correctly", "[cwSurveyNotesConcatModel]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    cwCave* cave = new cwCave();
    cave->setName("test");

    cwTrip* trip1 = new cwTrip();
    trip1->setName("trip 1");
    cave->addTrip(trip1);

    cwTrip* trip2 = new cwTrip();
    trip2->setName("trip 2");
    cave->addTrip(trip2);

    project->cavingRegion()->addCave(cave);

    // Add a note to trip1
    const QString png1 = testcasesDatasetPath("test_cwTextureUploadTask/PhakeCave.PNG");
    trip1->notes()->addFromFiles({ QUrl::fromLocalFile(png1) });
    rootData->futureManagerModel()->waitForFinished();
    REQUIRE(trip1->notes()->rowCount() == 1);

    // Add two notes to trip2
    const QString png2 = testcasesDatasetPath("test_cwNote/testpage.png");
    const QString glb = testcasesDatasetPath("test_cwSurveyNotesConcatModel/bones.glb");
    trip2->notes()->addFromFiles({ QUrl::fromLocalFile(png2) });
    trip2->notesLiDAR()->addFromFiles({ QUrl::fromLocalFile(glb) });
    rootData->futureManagerModel()->waitForFinished();
    REQUIRE(trip2->notes()->rowCount() == 1);
    REQUIRE(trip2->notesLiDAR()->rowCount() == 1);

    cwSurveyNotesConcatModel concat;
    concat.setTrip(trip1);
    REQUIRE(concat.rowCount() == 1);

    // Switch to trip2 — must not assert on stale indices
    concat.setTrip(trip2);
    REQUIRE(concat.rowCount() == 2);

    // All indices must be valid after the switch
    for (int i = 0; i < concat.rowCount(); ++i) {
        const QModelIndex idx = concat.index(i, 0);
        REQUIRE(idx.isValid());
        REQUIRE(!concat.data(idx, cwSurveyNoteModelBase::PathRole).toString().isEmpty());
    }

    // Switch back to trip1
    concat.setTrip(trip1);
    REQUIRE(concat.rowCount() == 1);
    REQUIRE(concat.index(0, 0).isValid());

    // Clear to null
    concat.setTrip(nullptr);
    REQUIRE(concat.rowCount() == 0);
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

    const QString pngPath = copyToTempFolder(testcasesDatasetPath("test_cwTextureUploadTask/PhakeCave.PNG"));
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
