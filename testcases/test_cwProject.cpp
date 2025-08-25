//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwProject.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "TestHelper.h"
#include "cwImageProvider.h"
#include "cwRootData.h"
#include "cwSurveyNoteModel.h"
#include "cwTaskManagerModel.h"
#include "cwFutureManagerModel.h"
#include "cwImageDatabase.h"
#include "cwPDFSettings.h"
#include "cwNote.h"
#include "cwSaveLoad.h"
#include "cwTeam.h"

//Qt includes
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlResult>
#include <QSqlRecord>

TEST_CASE("cwProject isModified should work correctly", "[cwProject]") {
    cwProject project;
    CHECK(project.isModified() == false);

    cwCave* cave1 = new cwCave();
    cave1->setName("cave1");
    project.cavingRegion()->addCave(cave1);

    CHECK(project.isModified() == true);

    QString testFile = prependTempFolder("test_cwProject.cw");
    QFile::remove(testFile);

    //Fixme: this was calling save as
    REQUIRE(false);

    // project.saveAs(testFile);
    project.waitSaveToFinish();

    CHECK(project.isModified() == false);

    project.newProject();
    CHECK(project.isModified() == false);

    project.loadOrConvert(testFile);
    project.waitLoadToFinish();
    CHECK(project.isModified() == false);

    REQUIRE(project.cavingRegion()->caveCount() == 1);

    project.cavingRegion()->cave(0)->addTrip();
    CHECK(project.isModified() == true);

    //Fixme: this was calling save
    REQUIRE(false);

    // project.save();
    project.waitSaveToFinish();
    CHECK(project.isModified() == false);

    SECTION("Load file") {
        //If this fails, this is probably because of a version change, or other save changes
        fileToProject(&project, "://datasets/network.cw");
        project.waitLoadToFinish();
        CHECK(project.isModified() == false);

        REQUIRE(project.cavingRegion()->caveCount() == 1);
        REQUIRE(project.cavingRegion()->cave(0)->tripCount() == 1);

        cwTrip* firstTrip = project.cavingRegion()->cave(0)->trip(0);
        firstTrip->setName("Sauce!");
        CHECK(project.isModified() == true);
    }
}

TEST_CASE("Image data should save and load correctly", "[cwProject]") {

    qRegisterMetaType<QList <cwImage> >("QList<cwImage>");

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    //Fixme: this was calling saveAs
    REQUIRE(false);

    // project->saveAs(prependTempFolder("imageTest-" + QUuid::createUuid().toString().left(5)));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    cave->addTrip();
    auto trip = cave->trip(0);

    QString filename = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    QImage originalImage(filename);

    trip->notes()->addFromFiles({QUrl("file:/" + filename)});

    rootData->futureManagerModel()->waitForFinished();

    SECTION("Is Modifed shouldn't effect the save and load") {
        //Make sure is modified doesn't modify the underlying file
        CHECK(project->isModified() == true);
    }

    REQUIRE(trip->notes()->notes().size() == 1);

    cwImage image = trip->notes()->notes().first()->image();
    cwImageProvider provider;
    provider.setProjectPath(project->filename());
    QImage sqlImage = provider.image(image.path());

    CHECK(!sqlImage.isNull());
    CHECK(originalImage == sqlImage);
}

TEST_CASE("Images should load correctly", "[cwProject]") {

    QSize size(1024, 1024);

    struct Image {
        QColor topLeftColor;
        QSize size;
        QUrl filename;
    };

    auto image = [size](const QColor& color)->Image {
        QImage image(size, QImage::Format_ARGB32);
        image.fill(color);
        QString imageFilename = QDir::tempPath() + "/" + QString("cavewhere-cwProject-image%1%2%3.png").arg(color.red()).arg(color.green()).arg(color.blue());
        REQUIRE(image.save(imageFilename, "png"));
        return {color, size, QUrl::fromLocalFile(imageFilename)};
    };

    QVector<QColor> imageColors = {"red", "green", "blue"};
    QList<Image> testImages;
    std::transform(imageColors.begin(), imageColors.end(), std::back_inserter(testImages), image);

    QString crashMapPath = "://datasets/test_cwProject/crashMap.png";
    QImage crashMap(crashMapPath);
    auto y = crashMap.size().height() - 1;
    testImages += Image {crashMap.pixelColor(0, y),
        crashMap.size(),
        QUrl::fromLocalFile(copyToTempFolder(crashMapPath))
    };

    QVector<QUrl> filenames;
    std::transform(testImages.begin(), testImages.end(), std::back_inserter(filenames),
                   [](const Image& image) {
                       return image.filename;
                   }
                   );

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    int checked = 0;
    project->addImages(filenames, [&checked, testImages, project, size](QList<cwImage> images){
        REQUIRE(images.size() == testImages.size());

        //Load the image and check that it's in the correct order
        for(int i = 0; i < images.size(); i++) {
            cwTextureUploadTask uploadTask;
            uploadTask.setImage(images.at(i));
            uploadTask.setProjectFilename(project->filename());
            uploadTask.setType(cwTextureUploadTask::OpenGL_RGBA);
            auto future = uploadTask.mipmaps();
            auto imageData = future.result();

            CHECK(imageData.image.size() == testImages.at(i).size);
            CHECK(imageData.image.pixelColor(0, 0) == testImages.at(i).topLeftColor);
        }

        checked++;
    });

    rootData->futureManagerModel()->waitForFinished();
    CHECK(checked == 1);
    CHECK(filenames.size() > 0);
}

TEST_CASE("Images should be removed correctly", "[cwProject]") {

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    QList<QUrl> filenames {
        QUrl::fromLocalFile(copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG")),
        QUrl::fromLocalFile(copyToTempFolder("://datasets/test_cwProject/crashMap.png"))
    };

    QList<cwImage> loadedImages;
    project->addImages(filenames, [&loadedImages](QList<cwImage> images){
        loadedImages += images;
    });

    rootData->futureManagerModel()->waitForFinished();

    REQUIRE(loadedImages.size() == 2);
    auto firstImage = loadedImages.at(0);
    auto secondImage = loadedImages.at(1);

    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", QString("ImageRemoveTest"));
    database.setDatabaseName(project->filename());
    REQUIRE(database.open());

    auto idExists = [database](int id) {
        INFO("idExists:" << id);
        QString SQL = QString("select count(*) from Images where id = %1").arg(id);

        INFO("sql:" << SQL.toStdString());
        INFO("database:" << database.databaseName());

        REQUIRE(database.isOpen());
        REQUIRE(database.isValid());

        QSqlQuery query(database);
        REQUIRE(query.prepare(SQL));

        REQUIRE(query.exec());
        REQUIRE(query.first());

        auto record = query.record();
        return record.value(0).toInt() == 1;
    };

    auto idNotExists = [idExists](int id) {
        return !idExists(id);
    };

    auto imageIds = [](const cwImage& image) {
        QList<int> ids {
            image.original(),
            image.icon(),
        };

        ids.append(image.mipmaps());
        return ids;
    };

    auto imageTestFunc = [imageIds](const cwImage& image, auto func) {
        auto ids = imageIds(image);

        return std::accumulate(ids.begin(), ids.end(), true,
                               [func](bool current, int id)
                               {
                                   return current && func(id);
                               });
    };

    auto imageExists = [idExists, imageTestFunc](const cwImage& image) {
        return imageTestFunc(image, idExists);
    };

    auto imageNotExists = [idNotExists, imageTestFunc](const cwImage& image) {
        return imageTestFunc(image, idNotExists);
    };

    CHECK(imageExists(firstImage));
    CHECK(imageExists(secondImage));

    cwImageDatabase imageDatabase(project->filename());
    imageDatabase.removeImage(secondImage);

    CHECK(imageNotExists(secondImage));
    CHECK(imageExists(firstImage));

    imageDatabase.removeImages({firstImage.original()});

    QList<int> ids = {
        firstImage.icon()
};
ids += firstImage.mipmaps();

for(auto id : ids){
    INFO("id:" << id);
    CHECK(idExists(id));
}

CHECK(idNotExists(firstImage.original()));

database.close();
}

TEST_CASE("cwProject should add PDF correctly", "[cwProject]") {
    if(cwProject::supportedImageFormats().contains("pdf")) {
        auto rootData = std::make_unique<cwRootData>();
        auto project = rootData->project();

        QList<QUrl> filenames {
            QUrl::fromLocalFile(copyToTempFolder("://datasets/test_cwPDFConverter/2page-test.pdf"))
        };

        struct Row {
            QList<QSize> pageSizes;
            int resolutionPPI;
        };

        QList<Row> rows = {
            {
                {
                    QSize(139, 139),
                    QSize(208, 139)
                },
                100
            },

            {
                {
                    QSize(417, 417),
                    QSize(625, 417)
                },
                300
            }
        };

        for(auto row : rows) {
            cwPDFSettings::instance()->setResolutionImport(row.resolutionPPI);

            project->addImages(filenames, [row](QList<cwImage> images){
                REQUIRE(images.size() == 2);
                CHECK(images.at(0).isOriginalValid());
                CHECK(images.at(0).isIconValid());
                CHECK(row.pageSizes.contains(images.at(0).originalSize()));
                CHECK(row.pageSizes.contains(images.at(1).originalSize()));
            });

            rootData->futureManagerModel()->waitForFinished();
        }
    }
}

TEST_CASE("cwProject should detect the correct file type", "[cwProject]") {
    //Older sqlite project
    QString datasetFile = copyToTempFolder(":/datasets/test_cwProject/Phake Cave 3000.cw");
    auto project = std::make_unique<cwProject>();
    CHECK(project->projectType(datasetFile) == cwProject::SqliteFileType);

    //A file based file
    datasetFile = copyToTempFolder(":/datasets/test_cwProject/v7.cw");
    CHECK(project->projectType(datasetFile) == cwProject::GitFileType);

    //Empty file
    QTemporaryFile tempFile;
    tempFile.open();
    datasetFile = tempFile.fileName();
    CHECK(project->projectType(datasetFile) == cwProject::UnknownFileType);

    //File with random stuff in it
    tempFile.write("Test random data");
    tempFile.close();
    CHECK(project->projectType(datasetFile) == cwProject::UnknownFileType);
}

TEST_CASE("Test save changes", "[cwProject]") {

    SECTION("Simple cave name change") {
        //Change the name of the cave
        auto project = std::make_unique<cwProject>();
        project->waitSaveToFinish();

        INFO("Project name:" << project->filename().toStdString());
        CHECK(project->isTemporaryProject());
        CHECK(QFileInfo::exists(project->filename()));

        //Add the cave
        cwCave* cave = new cwCave();
        cave->setName("test");
        project->cavingRegion()->addCave(cave);
        project->waitSaveToFinish();

        //Check no-name cave
        CHECK(QFileInfo::exists(project->filename()));
        // CHECK(project->filename() == cwSaveLoad::projectAbsolutePath(project.get()));

        auto testFilename = cwSaveLoad::absolutePath(cave);
        QFileInfo info(testFilename);
        auto oldDir = info.absoluteDir();
        oldDir.cdUp();

        CHECK(QFileInfo::exists(cwSaveLoad::absolutePath(cave)));

        //Test renaming the cave
        cave->setName("test2");
        project->waitSaveToFinish();

        CHECK(QFileInfo::exists(cwSaveLoad::absolutePath(cave)));
        CHECK(cwSaveLoad::absolutePath(cave).toStdString() == oldDir.filePath("test2/test2.cwcave").toStdString());
    }

    SECTION("Simple cave and trip") {
        //Change the name of the cave
        auto project = std::make_unique<cwProject>();
        project->waitSaveToFinish();

        INFO("Project name:" << project->filename().toStdString());
        CHECK(project->isTemporaryProject());
        CHECK(QFileInfo::exists(project->filename()));

        //Add the cave
        cwCave* cave = new cwCave();
        cave->setName("test");

        //Add a trip
        cwTrip* trip = new cwTrip();
        trip->setName("test trip");
        cave->addTrip(trip);

        //Add them all
        project->cavingRegion()->addCave(cave);

        project->waitSaveToFinish();

        CHECK(QFileInfo::exists(cwSaveLoad::absolutePath(cave)));
        CHECK(QFileInfo::exists(cwSaveLoad::absolutePath(trip)));

        SECTION("Rename the cave") {
            auto testFilename = cwSaveLoad::absolutePath(cave);
            QFileInfo info(testFilename);
            auto oldDir = info.absoluteDir();
            oldDir.cdUp();

            //Test renaming the cave
            cave->setName("test2");

            project->waitSaveToFinish();

            CHECK(QFileInfo::exists(cwSaveLoad::absolutePath(cave)));
            CHECK(QFileInfo::exists(cwSaveLoad::absolutePath(trip)));
            CHECK(cwSaveLoad::absolutePath(cave).toStdString() == oldDir.filePath("test2/test2.cwcave").toStdString());
            CHECK(cwSaveLoad::absolutePath(trip).toStdString() == oldDir.filePath("test2/trips/test trip/test trip.cwtrip").toStdString());
        }

        SECTION("Make sure trip calibration changes are saved") {
            trip->calibrations()->setDeclination(12.0);
            project->waitSaveToFinish();

            CHECK(QFileInfo::exists(cwSaveLoad::absolutePath(trip)));

            auto loadingProject = std::make_unique<cwProject>();
            loadingProject->loadOrConvert(project->filename());
            loadingProject->waitLoadToFinish();

            REQUIRE(loadingProject->cavingRegion()->caveCount() == 1);
            auto loadCave = loadingProject->cavingRegion()->cave(0);

            REQUIRE(loadCave->tripCount() == 1);
            auto loadTrip = loadCave->trip(0);

            CHECK(trip->calibrations()->declination() == 12.0);
            CHECK(loadTrip->calibrations()->declination() == 12.0);
        }
    }
}

TEST_CASE("Trip calibration persistence", "[cwProject]") {

    // Fresh project with one cave + one trip
    auto project = std::make_unique<cwProject>();
    project->waitSaveToFinish();

    cwCave* cave = new cwCave();
    cave->setName("test-cal");

    cwTrip* trip = new cwTrip();
    trip->setName("trip-cal");
    cave->addTrip(trip);

    project->cavingRegion()->addCave(cave);
    project->waitSaveToFinish();

    REQUIRE(QFileInfo::exists(cwSaveLoad::absolutePath(trip)));

    SECTION("Booleans persist") {
        for(int i = 0; i < 100; i++) {
            auto calibration = trip->calibrations();
            calibration->setFrontSights(true);
            calibration->setBackSights(true);
            calibration->setCorrectedCompassFrontsight(true);
            calibration->setCorrectedClinoFrontsight(true);
            calibration->setCorrectedCompassBacksight(true);
            calibration->setCorrectedClinoBacksight(true);

            project->waitSaveToFinish();

            // Reload and verify
            auto loadingProject = std::make_unique<cwProject>();
            loadingProject->loadOrConvert(project->filename());
            loadingProject->waitLoadToFinish();

            REQUIRE(loadingProject->cavingRegion()->caveCount() == 1);
            auto loadCave = loadingProject->cavingRegion()->cave(0);
            REQUIRE(loadCave->tripCount() == 1);
            auto loadTrip = loadCave->trip(0);
            auto loadCalibration = loadTrip->calibrations();

            CHECK(loadCalibration->hasFrontSights());
            CHECK(loadCalibration->hasBackSights());
            CHECK(loadCalibration->hasCorrectedCompassFrontsight());
            CHECK(loadCalibration->hasCorrectedClinoFrontsight());
            CHECK(loadCalibration->hasCorrectedCompassBacksight());
            CHECK(loadCalibration->hasCorrectedClinoBacksight());
        }
    }

    SECTION("Not Booleans persist") {
        auto calibration = trip->calibrations();
        calibration->setFrontSights(false);
        calibration->setBackSights(false);
        calibration->setCorrectedCompassFrontsight(false);
        calibration->setCorrectedClinoFrontsight(false);
        calibration->setCorrectedCompassBacksight(false);
        calibration->setCorrectedClinoBacksight(false);

        project->waitSaveToFinish();

        // Reload and verify
        auto loadingProject = std::make_unique<cwProject>();
        loadingProject->loadOrConvert(project->filename());
        loadingProject->waitLoadToFinish();

        REQUIRE(loadingProject->cavingRegion()->caveCount() == 1);
        auto loadCave = loadingProject->cavingRegion()->cave(0);
        REQUIRE(loadCave->tripCount() == 1);
        auto loadTrip = loadCave->trip(0);
        auto loadCalibration = loadTrip->calibrations();

        CHECK(!loadCalibration->hasFrontSights());
        CHECK(!loadCalibration->hasBackSights());
        CHECK(!loadCalibration->hasCorrectedCompassFrontsight());
        CHECK(!loadCalibration->hasCorrectedClinoFrontsight());
        CHECK(!loadCalibration->hasCorrectedCompassBacksight());
        CHECK(!loadCalibration->hasCorrectedClinoBacksight());
    }

    SECTION("Numeric values persist") {
        const double tape = 1.0023;
        const double fComp = -0.75;
        const double fClino = 0.35;
        const double bComp = 1.10;
        const double bClino = -0.45;
        const double decl = 13.25;

        auto calibration = trip->calibrations();
        calibration->setTapeCalibration(tape);
        calibration->setFrontCompassCalibration(fComp);
        calibration->setFrontClinoCalibration(fClino);
        calibration->setBackCompassCalibration(bComp);
        calibration->setBackClinoCalibration(bClino);
        calibration->setDeclination(decl);

        project->waitSaveToFinish();

        auto loadingProject = std::make_unique<cwProject>();
        loadingProject->loadOrConvert(project->filename());
        loadingProject->waitLoadToFinish();

        auto loadTrip = loadingProject->cavingRegion()->cave(0)->trip(0);
        auto loadCalibration = loadTrip->calibrations();

        CHECK(loadCalibration->tapeCalibration() == tape);
        CHECK(loadCalibration->frontCompassCalibration() == fComp);
        CHECK(loadCalibration->frontClinoCalibration() == fClino);
        CHECK(loadCalibration->backCompassCalibration() == bComp);
        CHECK(loadCalibration->backClinoCalibration() == bClino);
        CHECK(loadCalibration->declination() == decl);
    }

    SECTION("Distance unit persists") {
        // Pick a unit that differs from the default
        const cwUnits::LengthUnit unit = cwUnits::LengthUnit::Feet;

        auto calibration = trip->calibrations();
        calibration->setDistanceUnit(unit);

        project->waitSaveToFinish();

        auto loadingProject = std::make_unique<cwProject>();
        loadingProject->loadOrConvert(project->filename());
        loadingProject->waitLoadToFinish();

        auto loadTrip = loadingProject->cavingRegion()->cave(0)->trip(0);
        auto loadCalibration = loadTrip->calibrations();

        CHECK(loadCalibration->distanceUnit() == unit);
    }

    SECTION("Last write wins for calibration fields") {
        auto calibration = trip->calibrations();

        calibration->setDeclination(5.0);
        project->waitSaveToFinish();

        calibration->setDeclination(9.5);
        calibration->setFrontCompassCalibration(0.2);
        project->waitSaveToFinish();

        // Final overwrite before reload
        calibration->setDeclination(12.75);
        calibration->setFrontCompassCalibration(-0.15);
        project->waitSaveToFinish();

        auto loadingProject = std::make_unique<cwProject>();
        loadingProject->loadOrConvert(project->filename());
        loadingProject->waitLoadToFinish();

        auto loadTrip = loadingProject->cavingRegion()->cave(0)->trip(0);
        auto loadCalibration = loadTrip->calibrations();

        CHECK(loadCalibration->declination() == 12.75);
        CHECK(loadCalibration->frontCompassCalibration() == -0.15);
    }
}

TEST_CASE("Trip team member role changes persist and trigger save", "[cwProject][cwTeam]") {
    auto project = std::make_unique<cwProject>();
    project->waitSaveToFinish();

    cwCave* cave = new cwCave();
    cave->setName("team-roles-cave");

    cwTrip* trip = new cwTrip();
    trip->setName("team-roles-trip");
    cave->addTrip(trip);

    project->cavingRegion()->addCave(cave);
    project->waitSaveToFinish();

    REQUIRE(QFileInfo::exists(cwSaveLoad::absolutePath(trip)));

    // Use the concrete model so we can call add/remove helpers
    cwTeam* const team = static_cast<cwTeam*>(trip->team());
    REQUIRE(team != nullptr);

    SECTION("Insert member and set NameRole + JobsRole → save + reload") {
        team->addTeamMember(); // appends at end
        const int rowInserted = team->rowCount() - 1;
        REQUIRE(rowInserted >= 0);

        // Set roles
        const QString nameInserted = QStringLiteral("Alice Example");
        const QStringList jobsInserted = QStringList{QStringLiteral("Sketcher"), QStringLiteral("Reader")};

        QModelIndex nameIndex = team->index(rowInserted, 0);
        REQUIRE(nameIndex.isValid());

        bool okName = team->setData(nameIndex, nameInserted, cwTeam::NameRole);
        REQUIRE(okName);

        bool okJobs = team->setData(nameIndex, jobsInserted, cwTeam::JobsRole);
        REQUIRE(okJobs);

        project->waitSaveToFinish();

        // Reload and verify both roles
        auto reloaded = std::make_unique<cwProject>();
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* const reloadedTrip = reloaded->cavingRegion()->cave(0)->trip(0);
        REQUIRE(reloadedTrip != nullptr);

        cwTeam* const reloadedTeam = static_cast<cwTeam*>(reloadedTrip->team());
        REQUIRE(reloadedTeam != nullptr);

        REQUIRE(reloadedTeam->rowCount() >= 1);
        QModelIndex reloadedIndex = reloadedTeam->index(rowInserted, 0);
        REQUIRE(reloadedIndex.isValid());

        CHECK(reloadedTeam->data(reloadedIndex, cwTeam::NameRole).toString() == nameInserted);
        CHECK(reloadedTeam->data(reloadedIndex, cwTeam::JobsRole).toStringList() == jobsInserted);
    }

    SECTION("Edit NameRole + JobsRole on existing member → save + reload") {
        // Ensure one row exists
        if (team->rowCount() == 0) {
            team->addTeamMember();
            const int newRow = team->rowCount() - 1;
            QModelIndex idx = team->index(newRow, 0);
            REQUIRE(idx.isValid());
            REQUIRE(team->setData(idx, QStringLiteral("Temp Member"), cwTeam::NameRole));
            REQUIRE(team->setData(idx, QStringList{QStringLiteral("Assistant")}, cwTeam::JobsRole));
        }

        const int rowToEdit = 0;
        QModelIndex editIndex = team->index(rowToEdit, 0);
        REQUIRE(editIndex.isValid());

        const QString newName = QStringLiteral("Bob Renamed");
        const QStringList newJobs = QStringList{QStringLiteral("Sketcher"), QStringLiteral("Instrument Person")};

        REQUIRE(team->setData(editIndex, newName, cwTeam::NameRole));
        REQUIRE(team->setData(editIndex, newJobs, cwTeam::JobsRole));

        project->waitSaveToFinish();

        auto reloaded = std::make_unique<cwProject>();
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* const reloadedTrip = reloaded->cavingRegion()->cave(0)->trip(0);
        REQUIRE(reloadedTrip != nullptr);

        cwTeam* const reloadedTeam = static_cast<cwTeam*>(reloadedTrip->team());
        REQUIRE(reloadedTeam != nullptr);

        REQUIRE(reloadedTeam->rowCount() >= 1);
        QModelIndex reloadedIndex = reloadedTeam->index(rowToEdit, 0);
        REQUIRE(reloadedIndex.isValid());

        CHECK(reloadedTeam->data(reloadedIndex, cwTeam::NameRole).toString() == newName);
        CHECK(reloadedTeam->data(reloadedIndex, cwTeam::JobsRole).toStringList() == newJobs);
    }

    SECTION("Remove member → save + reload") {
        // Ensure at least two members exist so removal changes the count
        while (team->rowCount() < 2) {
            team->addTeamMember();
            const int r = team->rowCount() - 1;
            QModelIndex idx = team->index(r, 0);
            REQUIRE(idx.isValid());
            REQUIRE(team->setData(idx, QStringLiteral("Member %1").arg(r + 1), cwTeam::NameRole));
            REQUIRE(team->setData(idx, QStringList{QStringLiteral("Helper")}, cwTeam::JobsRole));
        }

        const int previousCount = team->rowCount();
        const int removeRow = 0;
        team->removeTeamMember(removeRow);

        project->waitSaveToFinish();

        auto reloaded = std::make_unique<cwProject>();
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* const reloadedTrip = reloaded->cavingRegion()->cave(0)->trip(0);
        REQUIRE(reloadedTrip != nullptr);

        cwTeam* const reloadedTeam = static_cast<cwTeam*>(reloadedTrip->team());
        REQUIRE(reloadedTeam != nullptr);

        CHECK(reloadedTeam->rowCount() == previousCount - 1);
    }
}

TEST_CASE("Trip date persistence", "[cwProject][cwTrip]") {
    // Build project → cave → trip
    auto project = std::make_unique<cwProject>();
    project->waitSaveToFinish();

    cwCave* cave = new cwCave();
    cave->setName("date-persist-cave");

    cwTrip* trip = new cwTrip();
    trip->setName("date-persist-trip");
    cave->addTrip(trip);

    project->cavingRegion()->addCave(cave);
    project->waitSaveToFinish();

    REQUIRE(QFileInfo::exists(cwSaveLoad::absolutePath(trip)));

    SECTION("Initial date persists after reload") {
        const QDateTime dateUtc(QDate(2024, 5, 1), QTime());
        trip->setDate(dateUtc);
        project->waitSaveToFinish();

        auto reloaded = std::make_unique<cwProject>();
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* const loadTrip = reloaded->cavingRegion()->cave(0)->trip(0);
        REQUIRE(loadTrip != nullptr);

        const QDateTime loadedDate = loadTrip->date();
        CHECK(loadedDate.toMSecsSinceEpoch() == dateUtc.toMSecsSinceEpoch());
    }

    SECTION("Last write wins for date field") {
        const QDateTime firstDate(QDate(2023, 11, 23), QTime());
        const QDateTime secondDate(QDate(2024, 1, 2), QTime());
        const QDateTime finalDate(QDate(2025, 8, 25), QTime());

        trip->setDate(firstDate);
        project->waitSaveToFinish();

        trip->setDate(secondDate);
        project->waitSaveToFinish();

        trip->setDate(finalDate);
        project->waitSaveToFinish();

        auto reloaded = std::make_unique<cwProject>();
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* const loadTrip = reloaded->cavingRegion()->cave(0)->trip(0);
        REQUIRE(loadTrip != nullptr);

        const QDateTime loadedDate = loadTrip->date();
        CHECK(loadedDate.toMSecsSinceEpoch() == finalDate.toMSecsSinceEpoch());
    }
}

TEST_CASE("Survey chunk persistence", "[cwProject][cwTrip][cwSurveyChunk]") {
    auto project = std::make_unique<cwProject>();
    project->waitSaveToFinish();

    cwCave* cave = new cwCave();
    cave->setName("chunk-persist-cave");

    cwTrip* trip = new cwTrip();
    trip->setName("chunk-persist-trip");
    cave->addTrip(trip);

    project->cavingRegion()->addCave(cave);
    project->waitSaveToFinish();

    REQUIRE(QFileInfo::exists(cwSaveLoad::absolutePath(trip)));

    SECTION("Add two chunks with data → save → reload → verify ALL fields") {
        // ---------- Chunk 0 ----------
        trip->addNewChunk();
        cwSurveyChunk* c0 = trip->chunk(0);
        REQUIRE(c0 != nullptr);

        // Fill stations[0,1], LRUDs
        c0->setData(cwSurveyChunk::StationNameRole, 0, "A");
        c0->setData(cwSurveyChunk::StationLeftRole, 0, "0.10");
        c0->setData(cwSurveyChunk::StationRightRole, 0, "0.20");
        c0->setData(cwSurveyChunk::StationUpRole, 0, "0.30");
        c0->setData(cwSurveyChunk::StationDownRole, 0, "0.40");

        c0->setData(cwSurveyChunk::StationNameRole, 1, "B");
        c0->setData(cwSurveyChunk::StationLeftRole, 1, "0.50");
        c0->setData(cwSurveyChunk::StationRightRole, 1, "0.60");
        c0->setData(cwSurveyChunk::StationUpRole, 1, "0.70");
        c0->setData(cwSurveyChunk::StationDownRole, 1, "0.80");

        // Shot 0 (A->B), include = true
        c0->setData(cwSurveyChunk::ShotDistanceRole, 0, "10.50");
        c0->setData(cwSurveyChunk::ShotCompassRole, 0, "12.30");
        c0->setData(cwSurveyChunk::ShotBackCompassRole, 0, "12.40");
        c0->setData(cwSurveyChunk::ShotClinoRole, 0, "-2.50");
        c0->setData(cwSurveyChunk::ShotBackClinoRole, 0, "-2.60");
        c0->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, true);

        // Shot 1 (B->C), include = false + LRUDs for C
        {
            const cwStation from("B");
            const cwStation to("C");
            const cwShot shot("4.75","33.0","213.0","0.50","0.60");
            c0->appendShot(from, to, shot);

            const int shotIdx = 1;
            const int stnIdx = 2;

            c0->setData(cwSurveyChunk::ShotBackCompassRole, shotIdx, "213.2");
            c0->setData(cwSurveyChunk::ShotBackClinoRole, shotIdx, "0.55");
            c0->setData(cwSurveyChunk::ShotDistanceIncludedRole, shotIdx, false);

            c0->setData(cwSurveyChunk::StationNameRole, stnIdx, "C");
            c0->setData(cwSurveyChunk::StationLeftRole, stnIdx, "0.90");
            c0->setData(cwSurveyChunk::StationRightRole, stnIdx, "1.00");
            c0->setData(cwSurveyChunk::StationUpRole, stnIdx, "1.10");
            c0->setData(cwSurveyChunk::StationDownRole, stnIdx, "1.20");
        }

        // ---------- Chunk 1 ----------
        trip->addNewChunk();
        cwSurveyChunk* c1 = trip->chunk(1);
        REQUIRE(c1 != nullptr);

        c1->setData(cwSurveyChunk::StationNameRole, 0, "C");
        c1->setData(cwSurveyChunk::StationLeftRole, 0, "0.11");
        c1->setData(cwSurveyChunk::StationRightRole, 0, "0.22");
        c1->setData(cwSurveyChunk::StationUpRole, 0, "0.33");
        c1->setData(cwSurveyChunk::StationDownRole, 0, "0.44");

        c1->setData(cwSurveyChunk::StationNameRole, 1, "D");
        c1->setData(cwSurveyChunk::StationLeftRole, 1, "0.55");
        c1->setData(cwSurveyChunk::StationRightRole, 1, "0.66");
        c1->setData(cwSurveyChunk::StationUpRole, 1, "0.77");
        c1->setData(cwSurveyChunk::StationDownRole, 1, "0.88");

        // Shot 0 (C->D), include = true
        c1->setData(cwSurveyChunk::ShotDistanceRole, 0, "7.25");
        c1->setData(cwSurveyChunk::ShotCompassRole, 0, "101.0");
        c1->setData(cwSurveyChunk::ShotBackCompassRole, 0, "281.0");
        c1->setData(cwSurveyChunk::ShotClinoRole, 0, "3.00");
        c1->setData(cwSurveyChunk::ShotBackClinoRole, 0, "3.10");
        c1->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, true);

        // Shot 1 (D->E), include = false + LRUDs for E
        {
            const cwStation from("D");
            const cwStation to("E");
            const cwShot shot("2.10","280.0","100.0","-1.25","-1.20");
            c1->appendShot(from, to, shot);

            const int shotIdx = 1;
            const int stnIdx = 2;

            c1->setData(cwSurveyChunk::ShotBackCompassRole, shotIdx, "100.2");
            c1->setData(cwSurveyChunk::ShotBackClinoRole, shotIdx, "-1.22");
            c1->setData(cwSurveyChunk::ShotDistanceIncludedRole, shotIdx, false);

            c1->setData(cwSurveyChunk::StationNameRole, stnIdx, "E");
            c1->setData(cwSurveyChunk::StationLeftRole, stnIdx, "0.95");
            c1->setData(cwSurveyChunk::StationRightRole, stnIdx, "1.05");
            c1->setData(cwSurveyChunk::StationUpRole, stnIdx, "1.15");
            c1->setData(cwSurveyChunk::StationDownRole, stnIdx, "1.25");
        }

        project->waitSaveToFinish();

        // ---------- Reload & VERIFY EVERYTHING ----------
        auto reloaded = std::make_unique<cwProject>();
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* lt = reloaded->cavingRegion()->cave(0)->trip(0);
        REQUIRE(lt != nullptr);
        CHECK(lt->chunkCount() == 2);

        cwSurveyChunk* rc0 = lt->chunk(0);
        cwSurveyChunk* rc1 = lt->chunk(1);
        REQUIRE(rc0 != nullptr);
        REQUIRE(rc1 != nullptr);

        // Chunk 0: counts
        CHECK(rc0->stationCount() == 3);
        CHECK(rc0->shotCount() == 2);

        // Chunk 0: station 0 (A) + LRUD
        CHECK(rc0->data(cwSurveyChunk::StationNameRole, 0).toString().toStdString() == "A");
        CHECK(rc0->data(cwSurveyChunk::StationLeftRole, 0).toString().toStdString() == "0.10");
        CHECK(rc0->data(cwSurveyChunk::StationRightRole, 0).toString().toStdString() == "0.20");
        CHECK(rc0->data(cwSurveyChunk::StationUpRole, 0).toString().toStdString() == "0.30");
        CHECK(rc0->data(cwSurveyChunk::StationDownRole, 0).toString().toStdString() == "0.40");

        // Chunk 0: station 1 (B) + LRUD
        CHECK(rc0->data(cwSurveyChunk::StationNameRole, 1).toString().toStdString() == "B");
        CHECK(rc0->data(cwSurveyChunk::StationLeftRole, 1).toString().toStdString() == "0.50");
        CHECK(rc0->data(cwSurveyChunk::StationRightRole, 1).toString().toStdString() == "0.60");
        CHECK(rc0->data(cwSurveyChunk::StationUpRole, 1).toString().toStdString() == "0.70");
        CHECK(rc0->data(cwSurveyChunk::StationDownRole, 1).toString().toStdString() == "0.80");

        // Chunk 0: station 2 (C) + LRUD
        CHECK(rc0->data(cwSurveyChunk::StationNameRole, 2).toString().toStdString() == "C");
        CHECK(rc0->data(cwSurveyChunk::StationLeftRole, 2).toString().toStdString() == "0.90");
        CHECK(rc0->data(cwSurveyChunk::StationRightRole, 2).toString().toStdString() == "1.00");
        CHECK(rc0->data(cwSurveyChunk::StationUpRole, 2).toString().toStdString() == "1.10");
        CHECK(rc0->data(cwSurveyChunk::StationDownRole, 2).toString().toStdString() == "1.20");

        // Chunk 0: shot 0 fields (A->B)
        CHECK(rc0->data(cwSurveyChunk::ShotDistanceRole, 0).toString().toStdString() == "10.50");
        CHECK(rc0->data(cwSurveyChunk::ShotCompassRole, 0).toString().toStdString() == "12.30");
        CHECK(rc0->data(cwSurveyChunk::ShotBackCompassRole, 0).toString().toStdString() == "12.40");
        CHECK(rc0->data(cwSurveyChunk::ShotClinoRole, 0).toString().toStdString() == "-2.50");
        CHECK(rc0->data(cwSurveyChunk::ShotBackClinoRole, 0).toString().toStdString() == "-2.60");
        CHECK(rc0->data(cwSurveyChunk::ShotDistanceIncludedRole, 0).toBool() == true);

        // Chunk 0: shot 1 fields (B->C)
        CHECK(rc0->data(cwSurveyChunk::ShotDistanceRole, 1).toString().toStdString() == "4.75");
        CHECK(rc0->data(cwSurveyChunk::ShotCompassRole, 1).toString().toStdString() == "33.0");
        CHECK(rc0->data(cwSurveyChunk::ShotBackCompassRole, 1).toString().toStdString() == "213.2");
        CHECK(rc0->data(cwSurveyChunk::ShotClinoRole, 1).toString().toStdString() == "0.50");
        CHECK(rc0->data(cwSurveyChunk::ShotBackClinoRole, 1).toString().toStdString() == "0.55");
        CHECK(rc0->data(cwSurveyChunk::ShotDistanceIncludedRole, 1).toBool() == false);

        // Chunk 1: counts
        CHECK(rc1->stationCount() == 3);
        CHECK(rc1->shotCount() == 2);

        // Chunk 1: station 0 (C) + LRUD
        CHECK(rc1->data(cwSurveyChunk::StationNameRole, 0).toString().toStdString() == "C");
        CHECK(rc1->data(cwSurveyChunk::StationLeftRole, 0).toString().toStdString() == "0.11");
        CHECK(rc1->data(cwSurveyChunk::StationRightRole, 0).toString().toStdString() == "0.22");
        CHECK(rc1->data(cwSurveyChunk::StationUpRole, 0).toString().toStdString() == "0.33");
        CHECK(rc1->data(cwSurveyChunk::StationDownRole, 0).toString().toStdString() == "0.44");

        // Chunk 1: station 1 (D) + LRUD
        CHECK(rc1->data(cwSurveyChunk::StationNameRole, 1).toString().toStdString() == "D");
        CHECK(rc1->data(cwSurveyChunk::StationLeftRole, 1).toString().toStdString() == "0.55");
        CHECK(rc1->data(cwSurveyChunk::StationRightRole, 1).toString().toStdString() == "0.66");
        CHECK(rc1->data(cwSurveyChunk::StationUpRole, 1).toString().toStdString() == "0.77");
        CHECK(rc1->data(cwSurveyChunk::StationDownRole, 1).toString().toStdString() == "0.88");

        // Chunk 1: station 2 (E) + LRUD
        CHECK(rc1->data(cwSurveyChunk::StationNameRole, 2).toString().toStdString() == "E");
        CHECK(rc1->data(cwSurveyChunk::StationLeftRole, 2).toString().toStdString() == "0.95");
        CHECK(rc1->data(cwSurveyChunk::StationRightRole, 2).toString().toStdString() == "1.05");
        CHECK(rc1->data(cwSurveyChunk::StationUpRole, 2).toString().toStdString() == "1.15");
        CHECK(rc1->data(cwSurveyChunk::StationDownRole, 2).toString().toStdString() == "1.25");

        // Chunk 1: shot 0 fields (C->D)
        CHECK(rc1->data(cwSurveyChunk::ShotDistanceRole, 0).toString().toStdString() == "7.25");
        CHECK(rc1->data(cwSurveyChunk::ShotCompassRole, 0).toString().toStdString() == "101.0");
        CHECK(rc1->data(cwSurveyChunk::ShotBackCompassRole, 0).toString().toStdString() == "281.0");
        CHECK(rc1->data(cwSurveyChunk::ShotClinoRole, 0).toString().toStdString() == "3.00");
        CHECK(rc1->data(cwSurveyChunk::ShotBackClinoRole, 0).toString().toStdString() == "3.10");
        CHECK(rc1->data(cwSurveyChunk::ShotDistanceIncludedRole, 0).toBool() == true);

        // Chunk 1: shot 1 fields (D->E)
        CHECK(rc1->data(cwSurveyChunk::ShotDistanceRole, 1).toString().toStdString() == "2.10");
        CHECK(rc1->data(cwSurveyChunk::ShotCompassRole, 1).toString().toStdString() == "280.0");
        CHECK(rc1->data(cwSurveyChunk::ShotBackCompassRole, 1).toString().toStdString() == "100.2");
        CHECK(rc1->data(cwSurveyChunk::ShotClinoRole, 1).toString().toStdString() == "-1.25");
        CHECK(rc1->data(cwSurveyChunk::ShotBackClinoRole, 1).toString().toStdString() == "-1.22");
        CHECK(rc1->data(cwSurveyChunk::ShotDistanceIncludedRole, 1).toBool() == false);
    }

    SECTION("Edit one chunk: verify ALL changed fields survive reload") {
        trip->addNewChunk();
        cwSurveyChunk* c = trip->chunk(0);
        REQUIRE(c != nullptr);

        // Seed
        c->setData(cwSurveyChunk::StationNameRole, 0, "S1");
        c->setData(cwSurveyChunk::StationLeftRole, 0, "0.10");
        c->setData(cwSurveyChunk::StationRightRole, 0, "0.20");
        c->setData(cwSurveyChunk::StationUpRole, 0, "0.30");
        c->setData(cwSurveyChunk::StationDownRole, 0, "0.40");
        c->setData(cwSurveyChunk::StationNameRole, 1, "S2");

        c->setData(cwSurveyChunk::ShotDistanceRole, 0, "5.00");
        c->setData(cwSurveyChunk::ShotCompassRole, 0, "45.0");
        c->setData(cwSurveyChunk::ShotBackCompassRole, 0, "225.0");
        c->setData(cwSurveyChunk::ShotClinoRole, 0, "1.00");
        c->setData(cwSurveyChunk::ShotBackClinoRole, 0, "1.10");
        c->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, true);

        // Edits
        c->setData(cwSurveyChunk::StationNameRole, 0, "S1-renamed");
        c->setData(cwSurveyChunk::StationLeftRole, 0, "0.15");
        c->setData(cwSurveyChunk::StationRightRole, 0, "0.25");
        c->setData(cwSurveyChunk::StationUpRole, 0, "0.35");
        c->setData(cwSurveyChunk::StationDownRole, 0, "0.45");

        c->setData(cwSurveyChunk::ShotDistanceRole, 0, "5.25");
        c->setData(cwSurveyChunk::ShotCompassRole, 0, "46.5");
        c->setData(cwSurveyChunk::ShotBackCompassRole, 0, "226.2");
        c->setData(cwSurveyChunk::ShotClinoRole, 0, "0.75");
        c->setData(cwSurveyChunk::ShotBackClinoRole, 0, "1.05");
        c->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, false);

        project->waitSaveToFinish();

        auto reloaded = std::make_unique<cwProject>();
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* lt = reloaded->cavingRegion()->cave(0)->trip(0);
        cwSurveyChunk* rc = lt->chunk(0);

        CHECK(rc->data(cwSurveyChunk::StationNameRole, 0).toString().toStdString() == "S1-renamed");
        CHECK(rc->data(cwSurveyChunk::StationLeftRole, 0).toString().toStdString() == "0.15");
        CHECK(rc->data(cwSurveyChunk::StationRightRole, 0).toString().toStdString() == "0.25");
        CHECK(rc->data(cwSurveyChunk::StationUpRole, 0).toString().toStdString() == "0.35");
        CHECK(rc->data(cwSurveyChunk::StationDownRole, 0).toString().toStdString() == "0.45");

        CHECK(rc->data(cwSurveyChunk::ShotDistanceRole, 0).toString().toStdString() == "5.25");
        CHECK(rc->data(cwSurveyChunk::ShotCompassRole, 0).toString().toStdString() == "46.5");
        CHECK(rc->data(cwSurveyChunk::ShotBackCompassRole, 0).toString().toStdString() == "226.2");
        CHECK(rc->data(cwSurveyChunk::ShotClinoRole, 0).toString().toStdString() == "0.75");
        CHECK(rc->data(cwSurveyChunk::ShotBackClinoRole, 0).toString().toStdString() == "1.05");
        CHECK(rc->data(cwSurveyChunk::ShotDistanceIncludedRole, 0).toBool() == false);
    }

    SECTION("Remove first chunk → save → reload → verify remaining chunk ALL fields") {
        // Chunk 0
        trip->addNewChunk();
        {
            cwSurveyChunk* ch = trip->chunk(0);
            ch->setData(cwSurveyChunk::StationNameRole, 0, "X1");
            ch->setData(cwSurveyChunk::StationNameRole, 1, "X2");
            ch->setData(cwSurveyChunk::ShotDistanceRole, 0, "3.00");
            ch->setData(cwSurveyChunk::ShotCompassRole, 0, "10.0");
            ch->setData(cwSurveyChunk::ShotBackCompassRole, 0, "190.0");
            ch->setData(cwSurveyChunk::ShotClinoRole, 0, "-1.00");
            ch->setData(cwSurveyChunk::ShotBackClinoRole, 0, "-1.10");
            ch->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, true);

            // X2 -> X3
            const cwStation from("X2");
            const cwStation to("X3");
            const cwShot shot("1.50","15.0","195.0","-0.50","-0.40");
            ch->appendShot(from, to, shot);
            ch->setData(cwSurveyChunk::ShotBackCompassRole, 1, "195.2");
            ch->setData(cwSurveyChunk::ShotBackClinoRole, 1, "-0.45");
            ch->setData(cwSurveyChunk::ShotDistanceIncludedRole, 1, false);
        }

        // Chunk 1
        trip->addNewChunk();
        {
            cwSurveyChunk* ch = trip->chunk(1);
            ch->setData(cwSurveyChunk::StationNameRole, 0, "Y1");
            ch->setData(cwSurveyChunk::StationLeftRole, 0, "0.11");
            ch->setData(cwSurveyChunk::StationRightRole, 0, "0.22");
            ch->setData(cwSurveyChunk::StationUpRole, 0, "0.33");
            ch->setData(cwSurveyChunk::StationDownRole, 0, "0.44");

            ch->setData(cwSurveyChunk::StationNameRole, 1, "Y2");
            ch->setData(cwSurveyChunk::StationLeftRole, 1, "0.55");
            ch->setData(cwSurveyChunk::StationRightRole, 1, "0.66");
            ch->setData(cwSurveyChunk::StationUpRole, 1, "0.77");
            ch->setData(cwSurveyChunk::StationDownRole, 1, "0.88");

            ch->setData(cwSurveyChunk::ShotDistanceRole, 0, "8.00");
            ch->setData(cwSurveyChunk::ShotCompassRole, 0, "200.0");
            ch->setData(cwSurveyChunk::ShotBackCompassRole, 0, "20.0");
            ch->setData(cwSurveyChunk::ShotClinoRole, 0, "2.00");
            ch->setData(cwSurveyChunk::ShotBackClinoRole, 0, "2.10");
            ch->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, false);

            const cwStation from("Y2");
            const cwStation to("Y3");
            const cwShot shot("2.10","205.0","25.0","2.10","2.05");
            ch->appendShot(from, to, shot);
            ch->setData(cwSurveyChunk::ShotBackCompassRole, 1, "25.2");
            ch->setData(cwSurveyChunk::ShotBackClinoRole, 1, "2.06");
            ch->setData(cwSurveyChunk::ShotDistanceIncludedRole, 1, true);

            ch->setData(cwSurveyChunk::StationNameRole, 2, "Y3");
            ch->setData(cwSurveyChunk::StationLeftRole, 2, "0.95");
            ch->setData(cwSurveyChunk::StationRightRole, 2, "1.05");
            ch->setData(cwSurveyChunk::StationUpRole, 2, "1.15");
            ch->setData(cwSurveyChunk::StationDownRole, 2, "1.25");
        }

        REQUIRE(trip->chunkCount() == 2);
        trip->removeChunks(0, 0);
        project->waitSaveToFinish();

        auto reloaded = std::make_unique<cwProject>();
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* lt = reloaded->cavingRegion()->cave(0)->trip(0);
        CHECK(lt->chunkCount() == 1);

        cwSurveyChunk* r = lt->chunk(0);
        REQUIRE(r != nullptr);

        // Remaining chunk is Y*; verify ALL fields
        CHECK(r->stationCount() == 3);
        CHECK(r->shotCount() == 2);

        CHECK(r->data(cwSurveyChunk::StationNameRole, 0).toString().toStdString() == "Y1");
        CHECK(r->data(cwSurveyChunk::StationLeftRole, 0).toString().toStdString() == "0.11");
        CHECK(r->data(cwSurveyChunk::StationRightRole, 0).toString().toStdString() == "0.22");
        CHECK(r->data(cwSurveyChunk::StationUpRole, 0).toString().toStdString() == "0.33");
        CHECK(r->data(cwSurveyChunk::StationDownRole, 0).toString().toStdString() == "0.44");

        CHECK(r->data(cwSurveyChunk::StationNameRole, 1).toString().toStdString() == "Y2");
        CHECK(r->data(cwSurveyChunk::StationLeftRole, 1).toString().toStdString() == "0.55");
        CHECK(r->data(cwSurveyChunk::StationRightRole, 1).toString().toStdString() == "0.66");
        CHECK(r->data(cwSurveyChunk::StationUpRole, 1).toString().toStdString() == "0.77");
        CHECK(r->data(cwSurveyChunk::StationDownRole, 1).toString().toStdString() == "0.88");

        CHECK(r->data(cwSurveyChunk::StationNameRole, 2).toString().toStdString() == "Y3");
        CHECK(r->data(cwSurveyChunk::StationLeftRole, 2).toString().toStdString() == "0.95");
        CHECK(r->data(cwSurveyChunk::StationRightRole, 2).toString().toStdString() == "1.05");
        CHECK(r->data(cwSurveyChunk::StationUpRole, 2).toString().toStdString() == "1.15");
        CHECK(r->data(cwSurveyChunk::StationDownRole, 2).toString().toStdString() == "1.25");

        CHECK(r->data(cwSurveyChunk::ShotDistanceRole, 0).toString().toStdString() == "8.00");
        CHECK(r->data(cwSurveyChunk::ShotCompassRole, 0).toString().toStdString() == "200.0");
        CHECK(r->data(cwSurveyChunk::ShotBackCompassRole, 0).toString().toStdString() == "20.0");
        CHECK(r->data(cwSurveyChunk::ShotClinoRole, 0).toString().toStdString() == "2.00");
        CHECK(r->data(cwSurveyChunk::ShotBackClinoRole, 0).toString().toStdString() == "2.10");
        CHECK(r->data(cwSurveyChunk::ShotDistanceIncludedRole, 0).toBool() == false);

        CHECK(r->data(cwSurveyChunk::ShotDistanceRole, 1).toString().toStdString() == "2.10");
        CHECK(r->data(cwSurveyChunk::ShotCompassRole, 1).toString().toStdString() == "205.0");
        CHECK(r->data(cwSurveyChunk::ShotBackCompassRole, 1).toString().toStdString() == "25.2");
        CHECK(r->data(cwSurveyChunk::ShotClinoRole, 1).toString().toStdString() == "2.10");
        CHECK(r->data(cwSurveyChunk::ShotBackClinoRole, 1).toString().toStdString() == "2.06");
        CHECK(r->data(cwSurveyChunk::ShotDistanceIncludedRole, 1).toBool() == true);
    }
}



