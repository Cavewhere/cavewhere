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
