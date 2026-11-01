//Test includes
#include "TestHelper.h"
#include "LoadProjectHelper.h"

//Our includes
#include "cwRegionLoadTask.h"
#include "cwRootData.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwSurveyNetwork.h"
#include "cwErrorListModel.h"
#include "cavewhereVersion.h"
#include "cwSurveyNoteModel.h"
#include "cwImageProvider.h"
#include "cwTripCalibration.h"
#include "cwTeam.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwRunningProfileScrapViewMatrix.h"
#include "cwProjectedProfileScrapViewMatrix.h"
#include "cwScrapManager.h"
#include "cwTriangulatedData.h"
#include "cwStationPositionLookup.h"
#include "cwJobSettings.h"
#include "asyncfuture.h"

//Protobuf
#include "cavewhere.pb.h"
#include "qt.pb.h"

//std includes
#include <memory>
#include <iostream>
#include <optional>

//Qt includes
#include <QUuid>
#include <QDirIterator>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QBuffer>
#include <QImage>

//catch includes
#include <catch2/catch_test_macros.hpp>

//Survey networks should be saved in the cache, so this testcase no longer make sense
// TEST_CASE("Save / Load should work with cwSurveyNetwork", "[ProtoSaveLoad]") {

//     auto root = std::make_unique<cwRootData>();

//     cwCave* cave = new cwCave();
//     cwTrip* trip = new cwTrip();

//     cave->addTrip(trip);
//     root->region()->addCave(cave);

//     cwSurveyChunk* chunk = new cwSurveyChunk;

//     cwStation a1("a1");
//     cwStation a2("a2");
//     cwStation a3("a3");

//     cwShot a1_to_a2("100", "0.0", "0.0", "0.0", "0.0");
//     cwShot a2_to_a3("50", "0.0", "0.0", "0.0", "0.0");

//     chunk->appendShot(a1, a2, a1_to_a2);
//     chunk->appendShot(a2, a3, a2_to_a3);

//     trip->addChunk(chunk);

//     root->linePlotManager()->waitToFinish();

//     CHECK(root->project()->isTemporaryProject() == true);

//     auto filename = prependTempFolder(QString("test_cwSurveyNetwork-") + QUuid::createUuid().toString().remove(QRegularExpression("{|}|-")) + ".cw");
//     root->project()->saveAs(filename);
//     root->project()->waitSaveToFinish();

//     root->project()->newProject();
//     root->project()->waitSaveToFinish();

//     CHECK(root->region()->caveCount() == 0);

//     root->project()->loadOrConvert(filename);
//     root->project()->waitLoadToFinish();
//     REQUIRE(root->region()->caveCount() == 1);
//     CHECK(root->project()->isTemporaryProject() == false);

//     auto loadCave = root->region()->cave(0);
//     auto network = loadCave->network();

//     auto testStationNeigbors = [=](QString stationName, QStringList
//             neighbors) {
//         auto neighborsAtStation = network.neighbors(stationName);
//         auto foundNeighbors = QSet<QString>(neighborsAtStation.begin(), neighborsAtStation.end());
//         auto checkNeigbbors = QSet<QString>(neighbors.begin(), neighbors.end());
//         CHECK(foundNeighbors == checkNeigbbors);
//     };

//     testStationNeigbors("a1", {"a2"});
//     testStationNeigbors("a2", {"a1", "a3"});
//     testStationNeigbors("a3", {"a2"});
// }

TEST_CASE("Loading should report errors correctly", "[ProtoSaveLoad]") {
    auto root = std::make_unique<cwRootData>();

    SECTION("Bad file name that doesn't exist") {
        QString filename = "badFileName.cw";
        REQUIRE(!QFile::exists(filename));
        auto fullPath = QDir::current().absoluteFilePath(filename);

        auto errorModel = root->project()->errorModel();
        CHECK(errorModel->size() == 0);

        root->project()->loadOrConvert(filename);
        root->project()->waitLoadToFinish();

        REQUIRE(errorModel->size() == 1);
        CHECK(errorModel->at(0) == cwError(QString("Couldn't open '%1' because it doesn't exist").arg(filename), cwError::Fatal));

        CHECK(!QFile::exists(filename));
    }

    SECTION("Bad file that does exist") {
        auto allResourcePaths = [](const QString& root = QStringLiteral(":/")) {
            QStringList paths;
            QDirIterator it(
                root,
                QStringList{},                                // match all
                QDir::AllEntries | QDir::NoDotAndDotDot,     // include files and dirs
                QDirIterator::Subdirectories
                );
            while (it.hasNext()) {
                paths.push_back(it.next());
            }
            paths.sort();
            return paths;
        };

        auto filename = fileToProject(root->project(), testcasesDatasetPath("test_ProtoBufferSaveLoad/badFile.cw"));
        root->project()->waitLoadToFinish();
        auto errorModel = root->project()->errorModel();

        REQUIRE(errorModel->size() == 1);

        cwError expectedError = cwError(QStringLiteral("Couldn't open '%1' because it has a unknown file type. Is it corrupted!?").arg(filename), cwError::Fatal);
        INFO("Model:" << errorModel->at(0));
        INFO("expected:" << expectedError);

        CHECK(errorModel->at(0) == expectedError);

        CHECK(root->project()->isTemporaryProject());
    }

    SECTION("Detect thet protoBuf is corrupt") {
        fileToProject(root->project(), testcasesDatasetPath("test_ProtoBufferSaveLoad/corrupted.cw"));
        root->project()->waitLoadToFinish();
        auto errorModel = root->project()->errorModel();

        REQUIRE(errorModel->size() == 1);

        INFO("Model:" << errorModel->at(0));
        CHECK(errorModel->at(0) == cwError("Couldn't read proto buffer. Corrupted?!", cwError::Fatal));

        CHECK(root->project()->isTemporaryProject());
    }


#ifndef Q_OS_WIN
    //This testcase is broken on windows because removing the read permission still al
    SECTION("File doesn't have read permissions") {
        CHECK(root->project()->cavingRegion()->caveCount() == 0);

        auto filename = copyToTempFolder(testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

        QFile file(filename);
        CHECK(file.setPermissions(QFileDevice::WriteOwner | QFileDevice::WriteUser));

        QFileInfo info(filename);
        REQUIRE(!info.isReadable());

        root->project()->loadOrConvert(filename);
        root->project()->waitLoadToFinish();
        auto errorModel = root->project()->errorModel();

        REQUIRE(errorModel->size() == 1);

        QString expectErrorMessage = QString("Couldn't open '%1' because you don't have permission to read it").arg(filename);

        INFO("Error-at(0):" << errorModel->at(0));

        CHECK(errorModel->at(0) == cwError(expectErrorMessage, cwError::Fatal));

        CHECK(root->project()->isTemporaryProject());
    }
#endif

    SECTION("File doesn't have write permissions") {

        CHECK(root->project()->cavingRegion()->caveCount() == 0);

        auto filename = copyToTempFolder(testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

        QFile file(filename);
        CHECK(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::ReadUser));

        QFileInfo info(filename);
        REQUIRE(!info.isWritable());
        REQUIRE(info.isReadable());

        root->project()->loadOrConvert(filename);
        root->project()->waitLoadToFinish();
        auto errorModel = root->project()->errorModel();

        REQUIRE(errorModel->size() == 1);

        QString expectErrorMessage = QString("'%1' is a ReadOnly file. Copying it to a temporary file").arg(filename);

        CHECK(errorModel->at(0) == cwError(expectErrorMessage, cwError::Warning));

        CHECK(root->project()->filename().toStdString() != filename.toStdString());
        CHECK(root->project()->isTemporaryProject());
        CHECK(root->project()->cavingRegion()->caveCount() == 1);
        CHECK(root->project()->canSaveDirectly() == false);

    }

    root->futureManagerModel()->waitForFinished();
}

//This also test v3->v6
TEST_CASE("Save and load should work correctly for Projected Profile v3->v5", "[ProtoSaveLoad]") {

    auto fileCheck = [](QString filename, auto scrapCheckFunc, QString saveAsPath = {}) {
        auto root = std::make_unique<cwRootData>();

        root->project()->loadOrConvert(filename);
        root->project()->waitLoadToFinish();

        auto errorModel = root->project()->errorModel();
        REQUIRE(errorModel->size() == 0);

        //Test that it loades correctly, 6 stations, has noteStations, has polygon
        //and in running profile mode with running profile, with auto
        REQUIRE(root->project()->cavingRegion()->caveCount() == 1);
        auto cave = root->project()->cavingRegion()->cave(0);
        CHECK(cave->name().toStdString() == "My Cave");

        REQUIRE(cave->tripCount() == 1);
        auto trip = cave->trip(0);
        CHECK(trip->name().toStdString() == "Best Trip");

        auto calibration = trip->calibrations();
        CHECK(calibration->declination() == 1.0);
        CHECK(calibration->distanceUnit() == cwUnits::Meters);
        CHECK(calibration->tapeCalibration() == 0.03);

        CHECK(calibration->hasFrontSights() == true);
        CHECK(calibration->frontCompassCalibration() == 0.01);
        CHECK(calibration->frontClinoCalibration() == 0.02);
        CHECK(calibration->hasCorrectedCompassFrontsight() == false);
        CHECK(calibration->hasCorrectedClinoFrontsight() == false);

        CHECK(calibration->hasBackSights() == true);
        CHECK(calibration->backCompassCalibration() == -0.01);
        CHECK(calibration->backClinoCalibration() == -0.02);
        CHECK(calibration->hasCorrectedCompassBacksight() == true);
        CHECK(calibration->hasCorrectedClinoBacksight() == true);

        REQUIRE(trip->chunkCount() == 1);
        auto chunk = trip->chunk(0);

        struct Station {
            QString stationName;
            QString left;
            QString right;
            QString up;
            QString down;
        };

        QList<Station> stations {
            {"a1", "1", "2", "3", "4"},
            {"a2", {},{},{},{}},
            {"a3", {},{},{},{}},
            {"a4", {},{},{},{}},
            {"a5", {},{},{},{}},
            {"a6", {},{},{},{}}
        };

        REQUIRE(chunk->stationCount() == stations.size());

        auto checkStation = [=](int stationIndex) {
            auto chunkStation = chunk->station(stationIndex);
            auto station = stations.at(stationIndex);
            CHECK(chunkStation.name().toStdString() == station.stationName.toStdString());

            auto checkLRUD = [](auto stateFunc, auto valueFunc, const QString& checkValue) {
                if(!checkValue.isEmpty()) {
                    CHECK(stateFunc() == cwDistanceReading::State::Valid);
                    CHECK(valueFunc().value() == checkValue);
                } else {
                    CHECK(stateFunc() == cwDistanceReading::State::Empty);
                }
            };

            checkLRUD([chunkStation]() { return chunkStation.left().state();},
                      [chunkStation]() { return chunkStation.left(); },
                      station.left
                      );
            checkLRUD([chunkStation]() { return chunkStation.right().state();},
                      [chunkStation]() { return chunkStation.right(); },
                      station.right
                      );
            checkLRUD([chunkStation]() { return chunkStation.up().state();},
                      [chunkStation]() { return chunkStation.up(); },
                      station.up
                      );
            checkLRUD([chunkStation]() { return chunkStation.down().state();},
                      [chunkStation]() { return chunkStation.down(); },
                      station.down
                      );
        };

        struct Clino {

            enum Type {
                Up,
                Down,
                Value
            };

            Type type;
            QString value;
        };

        struct Shot {
            QString distance;
            QString compass;
            QString backCompass;
            Clino clino;
            Clino backClino;
        };

        QList<Shot> shots {
            { "10", {}, {}, {Clino::Down, {}}, {Clino::Up, {}}},
            { "7.3", "50", "50.1", {Clino::Value, "-75"}, {Clino::Value, "-74"}},
            { "4", "220", "219", {Clino::Value, "10"}, {Clino::Value, "11"}},
            { "6", "46", "46.2", {Clino::Value, "-85"}, {Clino::Value, "-84.5"}},
            { "21.5", "35", "34", {Clino::Value, "-78"}, {Clino::Value, "-77"}}
        };

        REQUIRE(chunk->shotCount() == shots.size());

        auto checkShot = [=](int shotIndex) {
            auto chunkShot = chunk->shot(shotIndex);
            auto shot = shots.at(shotIndex);
            CHECK(chunkShot.distance().value().toStdString() == shot.distance.toStdString());
            if(!shot.compass.isEmpty()) {
                CHECK(chunkShot.compass().state() == cwCompassReading::State::Valid);
                CHECK(shot.compass.toStdString() == chunkShot.compass().value().toStdString());
            } else {
                CHECK(chunkShot.compass().state() == cwCompassReading::State::Empty);
            }

            if(!shot.backCompass.isEmpty()) {
                CHECK(chunkShot.backCompass().state() == cwCompassReading::State::Valid);
                CHECK(shot.backCompass.toStdString() == chunkShot.backCompass().value().toStdString());
            } else {
                CHECK(chunkShot.backCompass().state() == cwCompassReading::State::Empty);
            }

            auto checkClino = [](Clino clino, auto clinoStateFunc, auto clinoValueFunc) {
                switch(clino.type) {
                case Clino::Up:
                    CHECK(clinoStateFunc() == cwClinoReading::State::Up);
                    break;
                case Clino::Down:
                    CHECK(clinoStateFunc() == cwClinoReading::State::Down);
                    break;
                case Clino::Value:
                    CHECK(clinoStateFunc() == cwClinoReading::State::Valid);
                    CHECK(clinoValueFunc().value().toStdString() == clino.value.toStdString());
                    break;
                }
            };

            INFO("Front Clino");
            checkClino(shot.clino,
                       [chunkShot]() { return chunkShot.clino().state(); },
            [chunkShot]() { return chunkShot.clino(); }
            );

            INFO("Back clino");
            checkClino(shot.backClino,
                       [chunkShot]() { return chunkShot.backClino().state(); },
            [chunkShot]() { return chunkShot.backClino(); }
            );

        };

        for(int i = 0; i < chunk->shotCount(); i++) {
            INFO("Shot i:" << i);
            checkShot(i);
        }

        REQUIRE(trip->team()->teamMembers().size() == 1);
        CHECK(trip->team()->teamMembers().at(0).name().toStdString() == "Sauce Man");
        CHECK(trip->team()->teamMembers().at(0).jobs() == QStringList({"Super", "Duper"}));

        REQUIRE(trip->notes()->notes().size() == 1);
        auto note = trip->notes()->notes().at(0);

        REQUIRE(note->scraps().size() == 1);
        auto scrap = note->scraps().at(0);
        scrapCheckFunc(scrap);

        root->futureManagerModel()->waitForFinished();
        root->project()->waitSaveToFinish();

        CHECK(root->project()->canSaveDirectly() == true);

        if (!saveAsPath.isEmpty()) {
            REQUIRE(root->project()->saveAs(saveAsPath));
            root->project()->waitSaveToFinish();
        }

        return root->project()->filename();
    };

    auto originalScrapCheck = [](cwScrap* scrap) {
        CHECK(scrap->type() == cwScrap::RunningProfile);
        CHECK(dynamic_cast<cwRunningProfileScrapViewMatrix*>(scrap->viewMatrix()) != nullptr);
        CHECK(scrap->stations().size() == 6);
        CHECK(scrap->polygon().size() > 0);

        scrap->setType(cwScrap::ProjectedProfile);
        CHECK(scrap->type() == cwScrap::ProjectedProfile);
        REQUIRE(dynamic_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix()) != nullptr);

        scrap->setCalculateNoteTransform(false);

        cwProjectedProfileScrapViewMatrix* profileView = static_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix());
        profileView->setAzimuth(225);
        profileView->setDirection(cwProjectedProfileScrapViewMatrix::RightToLeft);

        CHECK(profileView->azimuth() == 225);
        CHECK(profileView->direction() == cwProjectedProfileScrapViewMatrix::RightToLeft);
    };

    auto profileCheck = [](cwScrap* scrap) {
        CHECK(scrap->type() == cwScrap::ProjectedProfile);
        REQUIRE(dynamic_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix()) != nullptr);
        CHECK(scrap->stations().size() == 6);
        CHECK(scrap->polygon().size() > 0);

        CHECK(scrap->calculateNoteTransform() == false);
        cwProjectedProfileScrapViewMatrix* profileView = static_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix());
        CHECK(profileView->azimuth() == 225);
        CHECK(profileView->direction() == cwProjectedProfileScrapViewMatrix::RightToLeft);

    };

    const QString savePath = createTempSubdir() + QStringLiteral("/profile-roundtrip.cwproj");

    auto newFilename = fileCheck(testcasesDatasetPath("test_ProtoBufferSaveLoad/ProjectProfile-test-v3.cw"), originalScrapCheck, savePath);

    fileCheck(newFilename, profileCheck);
}



namespace {
//Pre-v6 region files store sight flags that can contradict their shots.
constexpr int kFirstVersionWithTrustedSightFlags = 6;
constexpr int kLegacyImageId = 1;
constexpr int kNoteSizePixels = 200;
constexpr int kNoteDotsPerMeter = 7874;
constexpr double kLegLength = 10.0;
constexpr double kNorth = 0.0;
constexpr double kEast = 90.0;
constexpr double kSouth = 180.0;
constexpr double kLevel = 0.0;

struct LegacyLeg {
    QString from;
    QString to;
    double compass;
};

struct LegacySightFlags {
    bool front;
    bool back;
};

enum class LegacySightSide {
    Front,
    Back
};

const QList<LegacyLeg> kPhakeLegs {
    {QStringLiteral("a1"), QStringLiteral("a2"), kNorth},
    {QStringLiteral("a2"), QStringLiteral("a3"), kEast},
    {QStringLiteral("a3"), QStringLiteral("a4"), kSouth}
};

void setLegacyPoint(QtProto::QPointF* protoPoint, const QPointF& point)
{
    protoPoint->set_x(point.x());
    protoPoint->set_y(point.y());
}

//A pre-v6 trip, Phake Cave style: the legs carry readings on one side only, in
//the v5 value + state fields, and the trip stores the given sight flags.
void addLegacyTrip(CavewhereProto::Cave* protoCave,
                   LegacySightFlags storedFlags,
                   LegacySightSide readingSide)
{
    auto protoTrip = protoCave->add_legacy_trips();
    protoTrip->set_name("alpha");
    auto date = protoTrip->mutable_date();
    date->set_year(2020);
    date->set_month(1);
    date->set_day(1);

    auto calibration = protoTrip->mutable_tripcalibration();
    calibration->set_distanceunit(CavewhereProto::Units::Meters);
    calibration->set_frontsights(storedFlags.front);
    calibration->set_backsights(storedFlags.back);

    auto chunk = protoTrip->add_chunks();
    chunk->add_legacy_stations()->set_name(kPhakeLegs.first().from.toStdString());
    for(const LegacyLeg& leg : kPhakeLegs) {
        chunk->add_legacy_stations()->set_name(leg.to.toStdString());

        auto shot = chunk->add_legacy_shots();
        shot->set_includedistance(true);
        shot->set_legacy_distance(kLegLength);
        if(readingSide == LegacySightSide::Front) {
            shot->set_legacy_compass(leg.compass);
            shot->set_legacy_clino(kLevel);
            //v5 states default to Valid, so an empty side is spelled out.
            shot->set_legacy_backcompassstate(CavewhereProto::CompassStates::Empty);
            shot->set_legacy_backclinostate(CavewhereProto::ClinoStates::Empty);
        } else {
            shot->set_legacy_backcompass(leg.compass);
            shot->set_legacy_backclino(kLevel);
            shot->set_legacy_compassstate(CavewhereProto::CompassStates::Empty);
            shot->set_legacy_clinostate(CavewhereProto::ClinoStates::Empty);
        }
    }
}

CavewhereProto::CavingRegion legacyPhakeRegion(LegacySightFlags storedFlags,
                                               LegacySightSide readingSide,
                                               std::optional<int> fileVersion)
{
    CavewhereProto::CavingRegion protoRegion;
    if(fileVersion.has_value()) {
        protoRegion.set_version(fileVersion.value());
    }

    auto protoCave = protoRegion.add_caves();
    protoCave->set_name("Phake Cave");
    protoCave->set_lengthunit(CavewhereProto::Units::Meters);
    protoCave->set_depthunit(CavewhereProto::Units::Meters);
    addLegacyTrip(protoCave, storedFlags, readingSide);
    return protoRegion;
}

//One plan note on the trip, with a square scrap tied to a1 and a2. The scrap's
//outline spans more than a dozen meters at the note's scale, so a carpeted
//scrap has many grid vertices inside it.
void addLegacyNoteWithScrap(CavewhereProto::Trip* protoTrip)
{
    auto protoNote = protoTrip->mutable_legacy_notemodel()->add_notes();

    auto protoImage = protoNote->mutable_image();
    protoImage->set_legacy_originalid(kLegacyImageId);
    protoImage->set_dotpermeter(kNoteDotsPerMeter);
    protoImage->mutable_size()->set_width(kNoteSizePixels);
    protoImage->mutable_size()->set_height(kNoteSizePixels);

    auto resolution = protoNote->mutable_imageresolution();
    resolution->set_value(kNoteDotsPerMeter);
    resolution->set_unit(CavewhereProto::Units::DotsPerMeter);

    auto protoScrap = protoNote->add_scraps();
    const QList<QPointF> outline {
        {0.1, 0.1}, {0.9, 0.1}, {0.9, 0.9}, {0.1, 0.9}
    };
    for(const QPointF& point : outline) {
        setLegacyPoint(protoScrap->add_outlinepoints(), point);
    }

    auto addNoteStation = [protoScrap](const QString& name, const QPointF& position) {
        auto noteStation = protoScrap->add_notestations();
        noteStation->mutable_legacy_name()->set_stringdata(name.toStdString());
        setLegacyPoint(noteStation->mutable_positiononnote(), position);
    };
    addNoteStation(QStringLiteral("a1"), QPointF(0.5, 0.2));
    addNoteStation(QStringLiteral("a2"), QPointF(0.5, 0.8));

    protoScrap->set_calculatenotetransform(true);
    protoScrap->set_type(CavewhereProto::Scrap::Plan);
}

QByteArray legacyNotePng()
{
    QImage image(kNoteSizePixels, kNoteSizePixels, QImage::Format_RGB32);
    image.fill(Qt::white);
    image.setDotsPerMeterX(kNoteDotsPerMeter);
    image.setDotsPerMeterY(kNoteDotsPerMeter);

    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return pngData;
}

//Writes a pre-v6 SQLite project with the legacy schema: the serialized region
//in ObjectData row 1, and each png in Images under kLegacyImageId onward.
bool writeLegacyProjectFile(const QString& filename,
                            const CavewhereProto::CavingRegion& protoRegion,
                            const QList<QByteArray>& pngImages = {})
{
    const QString connectionName = QStringLiteral("legacySightFlags-%1-%2")
                                       .arg(QCoreApplication::applicationPid())
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    const bool written = [&]() {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(filename);
        if(!database.open()) {
            return false;
        }

        QSqlQuery query(database);
        if(!query.exec(QStringLiteral("CREATE TABLE ObjectData (id INTEGER PRIMARY KEY AUTOINCREMENT,protoBuffer BLOB)"))
            || !query.exec(QStringLiteral("CREATE TABLE Images (id INTEGER PRIMARY KEY AUTOINCREMENT,type STRING,shouldDelete BOOL,width INTEGER,height INTEGER,dotsPerMeter INTEGER,imageData BLOB)"))) {
            return false;
        }

        const std::string regionBytes = protoRegion.SerializeAsString();
        query.prepare(QStringLiteral("INSERT INTO ObjectData (id, protoBuffer) VALUES (1, ?)"));
        query.addBindValue(QByteArray(regionBytes.data(), static_cast<qsizetype>(regionBytes.size())));
        if(!query.exec()) {
            return false;
        }

        int imageId = kLegacyImageId;
        for(const QByteArray& png : pngImages) {
            const QImage image = QImage::fromData(png, "PNG");
            query.prepare(QStringLiteral("INSERT INTO Images (id, type, shouldDelete, width, height, dotsPerMeter, imageData) VALUES (?, 'png', 0, ?, ?, ?, ?)"));
            query.addBindValue(imageId);
            query.addBindValue(image.width());
            query.addBindValue(image.height());
            query.addBindValue(image.dotsPerMeterX());
            query.addBindValue(png);
            if(!query.exec()) {
                return false;
            }
            imageId++;
        }

        database.close();
        return true;
    }();

    QSqlDatabase::removeDatabase(connectionName);
    return written;
}

//Loads the file the way the app opens a legacy project, which converts it and
//reloads the converted project.
void loadLegacyProject(cwRootData* rootData, const QString& filename)
{
    rootData->project()->loadOrConvert(filename);
    rootData->project()->waitLoadToFinish();

    const auto errors = rootData->project()->errorModel()->toList();
    INFO("Load errors:" << errors.size());
    REQUIRE(!cwError::containsFatal(errors));
    REQUIRE(rootData->project()->cavingRegion()->caveCount() == 1);
    REQUIRE(rootData->project()->cavingRegion()->cave(0)->tripCount() == 1);
}
}

TEST_CASE("Loading a pre-v6 region repairs sight flags that contradict the shots", "[ProtoSaveLoad][cwRegionLoadTask]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString filename = tempDir.filePath(QStringLiteral("legacy-sight-flags.cw"));

    auto rootData = std::make_unique<cwRootData>();

    auto loadedFlags = [&](const CavewhereProto::CavingRegion& protoRegion) {
        REQUIRE(writeLegacyProjectFile(filename, protoRegion));
        loadLegacyProject(rootData.get(), filename);
        const auto calibration = rootData->project()->cavingRegion()->cave(0)->trip(0)->calibrations();
        return LegacySightFlags{calibration->hasFrontSights(), calibration->hasBackSights()};
    };

    SECTION("A back-only trip whose shots carry front readings gets front sights") {
        const auto flags = loadedFlags(legacyPhakeRegion({false, true}, LegacySightSide::Front, std::nullopt));
        CHECK(flags.front == true);
        CHECK(flags.back == true);
    }

    SECTION("A trip with neither sight gets the side its shots carry") {
        const auto flags = loadedFlags(legacyPhakeRegion({false, false}, LegacySightSide::Front, std::nullopt));
        CHECK(flags.front == true);
        CHECK(flags.back == false);
    }

    SECTION("A front-only trip whose shots carry backsights gets back sights and keeps front sights") {
        const auto flags = loadedFlags(legacyPhakeRegion({true, false}, LegacySightSide::Back, std::nullopt));
        CHECK(flags.front == true);
        CHECK(flags.back == true);
    }

    SECTION("A version 6 file keeps its stored flags") {
        const auto flags = loadedFlags(legacyPhakeRegion({false, true}, LegacySightSide::Front,
                                                          kFirstVersionWithTrustedSightFlags));
        CHECK(flags.front == false);
        CHECK(flags.back == true);
    }

    rootData->futureManagerModel()->waitForFinished();
}

TEST_CASE("A pre-v6 project with stale sight flags solves and carpets its scraps", "[ProtoSaveLoad][cwRegionLoadTask]") {
    cwJobSettings::initialize();
    REQUIRE(cwJobSettings::instance()->automaticUpdate());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString filename = tempDir.filePath(QStringLiteral("Phake Cave.cw"));

    auto protoRegion = legacyPhakeRegion({false, true}, LegacySightSide::Front, std::nullopt);
    addLegacyNoteWithScrap(protoRegion.mutable_caves(0)->mutable_legacy_trips(0));
    REQUIRE(writeLegacyProjectFile(filename, protoRegion, {legacyNotePng()}));

    auto rootData = std::make_unique<cwRootData>();
    loadLegacyProject(rootData.get(), filename);

    auto cave = rootData->project()->cavingRegion()->cave(0);
    auto trip = cave->trip(0);
    CHECK(trip->calibrations()->hasFrontSights() == true);

    rootData->linePlotManager()->waitToFinish();
    rootData->scrapManager()->waitForFinish();

    CHECK(!cave->scopedPositions().positions().isEmpty());
    CHECK(cave->scopedPositions().hasPosition(QStringLiteral("a2")));

    REQUIRE(trip->notes()->notes().size() == 1);
    auto note = trip->notes()->notes().at(0);
    REQUIRE(note->scraps().size() == 1);
    auto scrap = note->scrap(0);

    auto results = rootData->scrapManager()->triangulateScraps({scrap}).scraps;
    REQUIRE(results.size() == 1);
    REQUIRE(AsyncFuture::waitForFinished(results.first().data));
    REQUIRE(results.first().data.resultCount() == 1);

    //An unsolved scrap morphs to its bare outline; a carpeted one fills with
    //grid vertices.
    const auto vertexCount = results.first().data.result().scrapGeometry().vertexCount();
    INFO("Vertex count:" << vertexCount << " outline points:" << scrap->points().size());
    CHECK(vertexCount > 4 * scrap->points().size());

    rootData->futureManagerModel()->waitForFinished();
}
