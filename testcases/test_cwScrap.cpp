/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include "LoadProjectHelper.h"
#include <catch2/catch_approx.hpp>

//Cavewhere includes
#include "cwProject.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwScrap.h"
#include "cwNoteTranformation.h"
#include "cwNote.h"
#include "cwSurveyNoteModel.h"
#include "cwCavingRegion.h"
#include "cwLinePlotManager.h"
#include "cwRootData.h"
#include "cwSurveyChunk.h"
#include "cwTripCalibration.h"
#include "cwTaskManagerModel.h"
#include "cwFutureManagerModel.h"
#include "cwProjectedProfileScrapViewMatrix.h"
#include "cwJobSettings.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGridConvergence.h"
#include "cwCoordinateTransform.h"
#include "cwGeoPoint.h"
#include "cwMath.h"
#include "cwNoteStation.h"
#include "cwSurveyNetwork.h"
#include "cwStationPositionLookup.h"

//Our includes
#include "TestHelper.h"

//Qt includes
#include <QtGlobal>
#include "cwSignalSpy.h"
#include <algorithm>

class TestRow {
public:
    TestRow(QString filename, double rotation, double scale) :
        Filename(filename),
        Rotation(rotation),
        Scale(scale),
        RotationEpsilon(1.0e-3),
        ScaleEpsilon(1.0e-4)
    {}

    TestRow(QString filename, double rotation, double scale, double rotationEpsilon, double scaleEpsilon) :
        Filename(filename),
        Rotation(rotation),
        Scale(scale),
        RotationEpsilon(rotationEpsilon),
        ScaleEpsilon(scaleEpsilon)
    {}

    TestRow(QString filename,
            double rotation,
            double scale,
            double rotationEpsilon,
            double scaleEpsilon,
            double profileAzimuth) :
        Filename(filename),
        Rotation(rotation),
        Scale(scale),
        RotationEpsilon(rotationEpsilon),
        ScaleEpsilon(scaleEpsilon),
        ProfileAzimuth(profileAzimuth)
    {}

    TestRow() :
        Rotation(0.0),
        Scale(0.0),
        RotationEpsilon(0.0),
        ScaleEpsilon(0.0),
        ProfileAzimuth(0.0)
    {}

    QString Filename;
    double Rotation;
    double Scale;
    double RotationEpsilon;
    double ScaleEpsilon;
    double ProfileAzimuth;

    QString CaveName = QStringLiteral("Cave 1");
    QString TripName = QStringLiteral("Trip 1");
    QString NoteName;
    int ScrapIndex = 0;
};

cwScrap* findScrap(const cwProject* project,
                   const QString& caveName,
                   const QString& tripName,
                   const QString& noteName,
                   int scrapIndex) {
    INFO("Project:" << project->filename());

    REQUIRE(project);
    REQUIRE(project->cavingRegion());
    auto caves = project->cavingRegion()->caves();
    REQUIRE(!caves.isEmpty());

    auto caveIt = caveName.isEmpty()
            ? caves.begin()
            : std::find_if(caves.begin(), caves.end(), [&caveName](const cwCave* cave) {
                  return cave->name().compare(caveName, Qt::CaseInsensitive) == 0;
              });
    REQUIRE(caveIt != caves.end());
    cwCave* cave = *caveIt;

    auto trips = cave->trips();
    REQUIRE(!trips.isEmpty());
    auto tripIt = tripName.isEmpty()
            ? trips.begin()
            : std::find_if(trips.begin(), trips.end(), [&tripName](const cwTrip* trip) {
                  return trip->name().compare(tripName, Qt::CaseInsensitive) == 0;
              });
    REQUIRE(tripIt != trips.end());
    cwTrip* trip = *tripIt;
    cwSurveyNoteModel* noteModel = trip->notes();

    INFO("Trip:" << trip->name());

    const auto notes = noteModel->notes();
    REQUIRE(!notes.isEmpty());

    auto noteIt = noteName.isEmpty()
            ? notes.begin()
            : std::find_if(notes.begin(), notes.end(), [&noteName](const cwNote* note) {
                  return note->name().compare(noteName, Qt::CaseInsensitive) == 0;
              });
    REQUIRE(noteIt != notes.end());
    cwNote* note = *noteIt;

    REQUIRE(note->scraps().size() >= scrapIndex + 1);
    cwScrap* scrap = note->scrap(scrapIndex);

    return scrap;
}

cwScrap* firstScrap(const cwProject* project) {
    return findScrap(project, QString(), QString(), QString(), 0);
}

cwScrap* findScrap(const cwProject* project, const TestRow& row) {
    return findScrap(project, row.CaveName, row.TripName, row.NoteName, row.ScrapIndex);
}

void checkScrapTransform(cwScrap* scrap, const TestRow& row) {
    INFO("Row filename:" << row.Filename.toStdString());
    cwNoteTranformation* transform = scrap->noteTransformation();

    double realScale = 1.0 / transform->scale();

    INFO("Calc Scale:" << realScale << " Row Scale:" << row.Scale );
    CHECK(realScale == Catch::Approx(row.Scale).epsilon(row.ScaleEpsilon));
    INFO("Calc Up:" << transform->northUp() << " Row Up:" << row.Rotation );
    CHECK(transform->northUp() == Catch::Approx(row.Rotation).epsilon(row.RotationEpsilon));

    if(scrap->type() == cwScrap::ProjectedProfile) {
        auto viewMatrix = dynamic_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix());
        REQUIRE(viewMatrix);
        CHECK(viewMatrix->azimuth() == Catch::Approx(row.ProfileAzimuth).margin(0.25));
    }
}

TEST_CASE("Auto Calculate Note Transform", "[cwScrap]") {

    QList<TestRow> rows;
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/runningProfileRotate.cw"), 87.8004635984, 176.349));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/runningProfileRotateMirror.cw"), 87.5044033263, 176.696));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/runningProfile.cw"), 358.11335805615993877, 175.592));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/runningProfileMirror.cw"), 357.70680855674373788, 176.721));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/runningProfileUpsideDown.cw"),  87.8188708214, 1729.652));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/ProjectProfile-test-v3.cw"),
                0.199, 255.962, 0.05, 0.05, 135.7));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/projectedProfile-90left.cw"),
                270.08, 255.66, 0.05, 0.05, 134.2));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/projectedProfile-90right.cw"),
                89.955, 255.667, 0.05, 0.05, 136.3));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/projectedProfile-180.cw"),
                        180, 255.193, 0.05, 0.05, 135.6));

    foreach(TestRow row, rows) {
        auto project = fileToProject(row.Filename);
        cwScrap* currentScrap = findScrap(project.get(), row);

        auto plotManager = std::make_unique<cwLinePlotManager>();
        plotManager->setRegion(project->cavingRegion());
        plotManager->waitToFinish();

        //Force recalculation
        CHECK(currentScrap->calculateNoteTransform() == false);
        currentScrap->setCalculateNoteTransform(true);

        checkScrapTransform(currentScrap, row);
    }
}

TEST_CASE("Exact Auto Calculate Note Transform", "[cwScrap]") {

    QList<TestRow> rows;
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/exact/profile-0rot-0mirror.cw"), 359.857, 5795.0, 0.05, 0.005));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/exact/profile-0rot-1mirror.cw"), 359.728, 5795.0, 0.06, 0.005));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/exact/profile-90rot-0mirror.cw"), 90, 5795.0, 0.05, 0.005));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/exact/profile-90rot-1mirror.cw"), 90, 5795.0, 0.05, 0.005));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/exact/profile-180rot-0mirror.cw"), 180, 5795.0, 0.05, 0.005));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/exact/profile-180rot-1mirror.cw"), 180, 5795.0, 0.05, 0.005));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/exact/profile-270rot-0mirror.cw"), 270, 5795.0, 0.05, 0.005));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/exact/profile-270rot-1mirror.cw"), 270, 5795.0, 0.05, 0.005));

    foreach(TestRow row, rows) {
        auto project = fileToProject(row.Filename);
        cwScrap* currentScrap = findScrap(project.get(), row);

        auto plotManager = std::make_unique<cwLinePlotManager>();
        plotManager->setRegion(project->cavingRegion());
        plotManager->waitToFinish();

        //Force recalculation
        INFO("Filename:" << row.Filename.toStdString());
        CHECK(currentScrap->calculateNoteTransform() == false);
        currentScrap->setCalculateNoteTransform(true);

        while(currentScrap->stations().size() >= 6) {
            checkScrapTransform(currentScrap, row);
            INFO("Removing station:" << currentScrap->station(currentScrap->stations().size() - 1).name().toStdString());
            currentScrap->removeStation(currentScrap->stations().size() - 1);
        }
    }
}

TEST_CASE("Check that auto calculate work outside of trip", "[cwScrap]") {
    QList<TestRow> rows;
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/exact/plan-seperate-trip.cw"), 30.13, 1606.3, 0.05, 0.005));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/exact/plan-seperate-trip-badSave.cw"), 30.13, 1606.3, 0.05, 0.005));

    foreach(TestRow row, rows) {
        row.TripName = QStringLiteral("Trip 2");
        auto root = std::make_unique<cwRootData>();
        root->settings()->jobSettings()->setAutomaticUpdate(true);
        root->scrapManager()->warpingSettings()->setGridResolutionMeters(10.0);
        root->scrapManager()->warpingSettings()->setUseShotInterpolationSpacing(false);
        root->scrapManager()->warpingSettings()->setUseMaxClosestStations(false);
        root->scrapManager()->warpingSettings()->setUseSmoothingRadius(false);
        CHECK(root->scrapManager()->automaticUpdate());

        fileToProject(root->project(), row.Filename);
        auto project = root->project();
        cwScrap* currentScrap = findScrap(project, row);

        // auto plotManager = root->linePlotManager();
        // plotManager->waitToFinish();

        // CHECK(!project->cavingRegion()->cave(0)->stationPositionLookup().isEmpty());

        // root->taskManagerModel()->waitForTasks();
        root->futureManagerModel()->waitForFinished();

        INFO("Finished after plotManager!");
        checkScrapTransform(currentScrap, row);

        currentScrap->setCalculateNoteTransform(false);

        //Force recalculation
        INFO("Filename:" << row.Filename.toStdString());
        CHECK(currentScrap->calculateNoteTransform() == false);
        currentScrap->setCalculateNoteTransform(true);
        CHECK(currentScrap->calculateNoteTransform() == true);
        checkScrapTransform(currentScrap, row);
    }
}

TEST_CASE("Auto calculate if survey station change position", "[cwScrap]") {
    QList<TestRow> rows;
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/exact/plan-seperate-trip.cw"), 354.13, 16063.06, 0.05, 0.005));

    foreach(TestRow row, rows) {
        row.TripName = QStringLiteral("Trip 2");
        auto root = std::make_unique<cwRootData>();
        root->settings()->jobSettings()->setAutomaticUpdate(true);
        root->scrapManager()->warpingSettings()->setGridResolutionMeters(10.0);
        root->scrapManager()->warpingSettings()->setUseShotInterpolationSpacing(false);
        root->scrapManager()->warpingSettings()->setUseMaxClosestStations(false);
        root->scrapManager()->warpingSettings()->setUseSmoothingRadius(false);
        CHECK(root->scrapManager()->automaticUpdate());

        fileToProject(root->project(), row.Filename);
        auto project = root->project();
        cwScrap* currentScrap = findScrap(project, row);

        REQUIRE(project->cavingRegion()->caves().size() > 0);
        REQUIRE(project->cavingRegion()->caves().first()->trips().size() > 0);
        REQUIRE(project->cavingRegion()->caves().first()->trips().first()->chunks().size() > 0);

        auto chunk = project->cavingRegion()->cave(0)->trip(0)->chunk(0);
        REQUIRE(chunk->shotCount() > 0);
        CHECK(chunk->data(cwSurveyChunk::ShotDistanceRole, 0).toDouble() == 100.0);
        chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, 1000);
        chunk->setData(cwSurveyChunk::ShotCompassRole, 0, 45);

        auto plotManager = root->linePlotManager();
        plotManager->waitToFinish();

        root->taskManagerModel()->waitForTasks();
        root->futureManagerModel()->waitForFinished();

        INFO("Finished after plotManager!");
        checkScrapTransform(currentScrap, row);

        currentScrap->setCalculateNoteTransform(false);

        //Force recalculation
        INFO("Filename:" << row.Filename.toStdString());
        CHECK(currentScrap->calculateNoteTransform() == false);
        currentScrap->setCalculateNoteTransform(true);
        CHECK(currentScrap->calculateNoteTransform() == true);
        checkScrapTransform(currentScrap, row);
    }
}

TEST_CASE("Auto calculate should work on projected profile azimuth", "[cwScrap]") {
    QList<TestRow> rows;
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/ProjectProfile-test-v3.cw"), 0.1997, 255.967, 0.05, 0.005, 135.7));

    foreach(TestRow row, rows) {
        auto root = std::make_unique<cwRootData>();
        root->settings()->jobSettings()->setAutomaticUpdate(true);
        fileToProject(root->project(), row.Filename);
        auto project = root->project();
        cwScrap* currentScrap = findScrap(project, row);

        auto plotManager = root->linePlotManager();
        plotManager->waitToFinish();

        root->taskManagerModel()->waitForTasks();
        root->futureManagerModel()->waitForFinished();

        //Force recalculation
        INFO("Filename:" << row.Filename.toStdString());
        CHECK(currentScrap->calculateNoteTransform() == false);
        currentScrap->setCalculateNoteTransform(true);
        CHECK(currentScrap->calculateNoteTransform() == true);
        checkScrapTransform(currentScrap, row);
    }
}

TEST_CASE("Auto calculate if the scrap type has changed", "[cwScrap]") {
    QList<TestRow> rows;
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/ProjectProfile-test-startRunning.cw"), 359.852, 257.162, 0.05, 0.005, 134.4));
    rows[0].CaveName = "My Cave";
    rows[0].TripName = "Best Trip";

    foreach(TestRow row, rows) {
        auto root = std::make_unique<cwRootData>();
        root->settings()->jobSettings()->setAutomaticUpdate(true);
        fileToProject(root->project(), row.Filename);
        auto project = root->project();
        cwScrap* currentScrap = findScrap(project, row);

        auto plotManager = root->linePlotManager();
        plotManager->waitToFinish();

        root->taskManagerModel()->waitForTasks();
        root->futureManagerModel()->waitForFinished();

        currentScrap->setCalculateNoteTransform(true);
        REQUIRE(currentScrap->type() == cwScrap::RunningProfile);

        TestRow runningProfileRow("", currentScrap->noteTransformation()->northUp(), 1.0 / currentScrap->noteTransformation()->scale(), 0.05, 0.005);
        checkScrapTransform(currentScrap, runningProfileRow);

        currentScrap->setType(cwScrap::ProjectedProfile);

        REQUIRE(dynamic_cast<cwProjectedProfileScrapViewMatrix*>(currentScrap->viewMatrix()));

        //Make sure it has change, because we've changed the type
        CHECK(runningProfileRow.Rotation != Catch::Approx(currentScrap->noteTransformation()->northUp()));
        CHECK(1.0 / runningProfileRow.Scale != Catch::Approx(currentScrap->noteTransformation()->scale()));

        //Change it back to running profile
        currentScrap->setType(cwScrap::RunningProfile);
        checkScrapTransform(currentScrap, runningProfileRow); //Shouldbe like the original

        //Change to projected profile
        currentScrap->setType(cwScrap::ProjectedProfile);
        REQUIRE(dynamic_cast<cwProjectedProfileScrapViewMatrix*>(currentScrap->viewMatrix()));
        auto projectedViewMatix2 = static_cast<cwProjectedProfileScrapViewMatrix*>(currentScrap->viewMatrix());
        CHECK(projectedViewMatix2->azimuth() == Catch::Approx(134.4).margin(0.15));
        CHECK(projectedViewMatix2->direction() == cwProjectedProfileScrapViewMatrix::LookingAt);
        checkScrapTransform(currentScrap, row);
    }
}

TEST_CASE("Manual scrap rotation uses declination during triangulation", "[cwScrap]") {
    auto project = fileToProject(testcasesDatasetPath("scrapAutoCalculate/runningProfile.cw"));
    cwScrap* scrap = firstScrap(project.get());
    REQUIRE(scrap);
    REQUIRE(scrap->parentNote());
    REQUIRE(scrap->parentNote()->parentTrip());

    cwTripCalibration* calibration = scrap->parentNote()->parentTrip()->calibrations();
    REQUIRE(calibration);

    cwNoteTranformation* noteTransform = scrap->noteTransformation();
    REQUIRE(noteTransform);

    scrap->setCalculateNoteTransform(false);

    const double startingDeclination = 0.0;
    const double endingDeclination = 12.5;
    calibration->setDeclinationManual(startingDeclination);
    noteTransform->setNorthUp(30.0);

    const double originalNorthUp = noteTransform->northUp();

    scrap->setType(cwScrap::Plan);
    const double originalEffectiveNorthUp = scrap->noteTransformAdjustedDeclination().north;
    CHECK(originalEffectiveNorthUp == Catch::Approx(cwNoteTranformation::northAdjustedForDeclination(originalNorthUp, calibration->declination())).epsilon(1e-6));

    calibration->setDeclinationManual(endingDeclination);

    const double updatedNorthUp = noteTransform->northUp();

    scrap->setType(cwScrap::Plan);
    const double updatedEffectiveNorthUp = scrap->noteTransformAdjustedDeclination().north;
    const double expectedEffectiveNorthUp = cwNoteTranformation::northAdjustedForDeclination(originalNorthUp, endingDeclination);

    CHECK(updatedNorthUp == Catch::Approx(originalNorthUp).epsilon(1e-6));
    CHECK(updatedEffectiveNorthUp == Catch::Approx(expectedEffectiveNorthUp).epsilon(1e-6));

    scrap->setType(cwScrap::RunningProfile);
    CHECK(scrap->noteTransformAdjustedDeclination().north == Catch::Approx(originalNorthUp).epsilon(1e-6));

    scrap->setType(cwScrap::ProjectedProfile);
    CHECK(scrap->noteTransformAdjustedDeclination().north == Catch::Approx(originalNorthUp).epsilon(1e-6));
}

TEST_CASE("Plan scrap removes grid convergence from the note transform", "[cwScrap]") {
    // Build a cave -> trip -> note -> scrap chain in memory and georeference
    // the cave with a projected coordinate system so grid convergence is
    // non-zero. Survex folds grid convergence into the plotted stations only
    // when declination is auto-computed (it leaves manual declination
    // verbatim, see survex datain.c get_declination). The note transform must
    // mirror that: remove convergence in addition to declination, but only
    // when the trip uses auto-declination.

    const QString utm13N = QStringLiteral("EPSG:32613"); // central meridian -105°

    cwCave cave;
    auto* trip = new cwTrip(&cave);
    cave.addTrip(trip);

    auto* note = new cwNote();
    trip->notes()->addNotes({note});

    auto* scrap = new cwScrap();
    note->addScrap(scrap);

    scrap->setType(cwScrap::Plan);
    scrap->setCalculateNoteTransform(false);
    scrap->noteTransformation()->setNorthUp(30.0);

    // Fix the cave east of the central meridian -> positive convergence.
    cwCoordinateTransform geoToUtm(QStringLiteral("EPSG:4326"), utm13N);
    REQUIRE(geoToUtm.isValid());
    const cwGeoPoint fixPoint = geoToUtm.transform(cwGeoPoint(-104.0, 40.015, 1655.0));

    cwFixStation fix;
    fix.setStationName(QStringLiteral("a1"));
    fix.setInputCS(utm13N);
    fix.setEasting(fixPoint.x);
    fix.setNorthing(fixPoint.y);
    fix.setElevation(fixPoint.z);
    cave.fixStations()->appendFixStation(fix);

    // The cave caches its convergence, computed the same way as the readout.
    auto expectedConvergence = cwGridConvergence::computeAt(fixPoint, utm13N);
    REQUIRE_FALSE(expectedConvergence.hasError());
    REQUIRE(expectedConvergence.value() > 0.0);
    CHECK(cave.gridConvergence()->angle() == Catch::Approx(expectedConvergence.value()).epsilon(1e-9));

    const double rawNorth = scrap->noteTransformation()->northUp();
    cwTripCalibration* calibration = trip->calibrations();

    SECTION("Auto declination: convergence is removed alongside declination") {
        trip->setDate(QDateTime(QDate(2020, 1, 1), QTime(0, 0)));
        calibration->setAutoDeclination(true);

        const double declination = calibration->declination();
        const double adjusted = scrap->noteTransformAdjustedDeclination().north;

        const double expected = cwWrapDegrees360(
            rawNorth - declination + expectedConvergence.value());
        CHECK(adjusted == Catch::Approx(expected).epsilon(1e-6));

        // The only difference from a declination-only adjustment is the
        // independently-computed grid convergence.
        const double declinationOnly =
            cwNoteTranformation::northAdjustedForDeclination(rawNorth, declination);
        CHECK(cwWrapDegrees360(adjusted - declinationOnly)
              == Catch::Approx(expectedConvergence.value()).margin(1e-6));
    }

    SECTION("Manual declination: convergence is left in (survex does not remove it)") {
        calibration->setAutoDeclination(false);
        calibration->setDeclinationManual(12.5);

        const double declination = calibration->declination();
        const double adjusted = scrap->noteTransformAdjustedDeclination().north;

        const double expected =
            cwNoteTranformation::northAdjustedForDeclination(rawNorth, declination);
        CHECK(adjusted == Catch::Approx(expected).epsilon(1e-6));
    }

    SECTION("Non-plan scraps ignore both declination and convergence") {
        trip->setDate(QDateTime(QDate(2020, 1, 1), QTime(0, 0)));
        calibration->setAutoDeclination(true);

        scrap->setType(cwScrap::RunningProfile);
        CHECK(scrap->noteTransformAdjustedDeclination().north
              == Catch::Approx(rawNorth).epsilon(1e-6));
    }
}

TEST_CASE("Auto-calculated note transform accounts for grid convergence", "[cwScrap]") {
    // updateNoteTransformation fits the note to the actual 3D plot, so the
    // effective transform (noteTransformAdjustedDeclination) is a purely
    // geometric alignment. Declination and grid convergence are storage-frame
    // bookkeeping that must cancel in the store/read round-trip. If auto-calc
    // fails to remove grid convergence at storage time, the effective alignment
    // of a georeferenced cave drifts from an identical un-georeferenced one by
    // the convergence angle — even though both fit the same plot.

    const QString utm13N = QStringLiteral("EPSG:32613"); // central meridian -105°

    // Builds an in-memory cave whose plot has two stations one grid-north of the
    // other, drawn as a single shot on the note, then auto-calculates and
    // returns the resolved (effective) north. The plot geometry is identical
    // whether or not the cave is georeferenced.
    auto effectiveAutoNorth = [&](bool georeference) -> double {
        auto cave = std::make_unique<cwCave>();
        auto* trip = new cwTrip(cave.get());
        cave->addTrip(trip);
        auto* note = new cwNote();
        trip->notes()->addNotes({note});
        auto* scrap = new cwScrap();
        note->addScrap(scrap);
        scrap->setType(cwScrap::Plan);
        scrap->setCalculateNoteTransform(true);

        cwStationPositionLookup lookup;
        lookup.setPosition(QStringLiteral("a1"), QVector3D(0.0f, 0.0f, 0.0f));
        lookup.setPosition(QStringLiteral("a2"), QVector3D(0.0f, 10.0f, 0.0f));
        cave->setStationPositionLookup(lookup);

        cwSurveyNetwork network;
        network.addShot(QStringLiteral("a1"), QStringLiteral("a2"));
        cave->setSurveyNetwork(network);

        cwNoteStation s1;
        s1.setName(QStringLiteral("a1"));
        s1.setPositionOnNote(QPointF(0.5, 0.4));
        cwNoteStation s2;
        s2.setName(QStringLiteral("a2"));
        s2.setPositionOnNote(QPointF(0.5, 0.6));
        scrap->setStations({s1, s2});

        if(georeference) {
            cwCoordinateTransform geoToUtm(QStringLiteral("EPSG:4326"), utm13N);
            REQUIRE(geoToUtm.isValid());
            const cwGeoPoint fixPoint = geoToUtm.transform(cwGeoPoint(-104.0, 40.015, 1655.0));

            cwFixStation fix;
            fix.setStationName(QStringLiteral("a1"));
            fix.setInputCS(utm13N);
            fix.setEasting(fixPoint.x);
            fix.setNorthing(fixPoint.y);
            fix.setElevation(fixPoint.z);
            cave->fixStations()->appendFixStation(fix);

            trip->setDate(QDateTime(QDate(2020, 1, 1), QTime(0, 0)));
            trip->calibrations()->setAutoDeclination(true);

            // Precondition: this cave really does have a non-trivial grid
            // rotation, otherwise the test proves nothing.
            REQUIRE(qAbs(cave->gridConvergence()->angle()) > 0.1);
        }

        scrap->updateNoteTransformation();
        return scrap->noteTransformAdjustedDeclination().north;
    };

    const double plain = effectiveAutoNorth(false);
    const double georeferenced = effectiveAutoNorth(true);

    INFO("plain=" << plain << " georeferenced=" << georeferenced);
    CHECK(georeferenced == Catch::Approx(plain).margin(1e-6));
}

TEST_CASE("Auto-calculate scrap transform handles declination correctly", "[cwScrap]") {
    auto root = std::make_unique<cwRootData>();
    root->settings()->jobSettings()->setAutomaticUpdate(true);
    fileToProject(root->project(),
                  testcasesDatasetPath("scrapAutoCalculate/ProjectProfile-test-v3.cw"));

    cwScrap* scrap = firstScrap(root->project());
    REQUIRE(scrap);
    REQUIRE(scrap->parentNote());
    REQUIRE(scrap->parentNote()->parentTrip());

    cwTripCalibration* calibration = scrap->parentNote()->parentTrip()->calibrations();
    REQUIRE(calibration);

    auto settle = [&]() {
        root->linePlotManager()->waitToFinish();
        root->taskManagerModel()->waitForTasks();
        root->futureManagerModel()->waitForFinished();
    };

    calibration->setDeclinationManual(0.0);
    scrap->setCalculateNoteTransform(true);
    settle();

    constexpr double kDeclination = 15.0;
    constexpr double kToleranceDegrees = 1.0;

    SECTION("ProjectedProfile: up direction must not rotate with declination") {
        scrap->setType(cwScrap::ProjectedProfile);
        scrap->updateNoteTransformation();
        const double upDec0 = scrap->noteTransformAdjustedDeclination().north;

        calibration->setDeclinationManual(kDeclination);
        settle();
        scrap->updateNoteTransformation();
        const double upDec15 = scrap->noteTransformAdjustedDeclination().north;

        INFO("upDec0=" << upDec0 << " upDec15=" << upDec15);
        CHECK(upDec15 == Catch::Approx(upDec0).margin(kToleranceDegrees));
    }

    SECTION("RunningProfile: up direction must not rotate with declination") {
        scrap->setType(cwScrap::RunningProfile);
        scrap->updateNoteTransformation();
        const double upDec0 = scrap->noteTransformAdjustedDeclination().north;

        calibration->setDeclinationManual(kDeclination);
        settle();
        scrap->updateNoteTransformation();
        const double upDec15 = scrap->noteTransformAdjustedDeclination().north;

        INFO("upDec0=" << upDec0 << " upDec15=" << upDec15);
        CHECK(upDec15 == Catch::Approx(upDec0).margin(kToleranceDegrees));
    }

    SECTION("Plan: effective up rotates by declination") {
        scrap->setType(cwScrap::Plan);
        scrap->updateNoteTransformation();
        const double upDec0 = scrap->noteTransformAdjustedDeclination().north;

        calibration->setDeclinationManual(kDeclination);
        settle();
        scrap->updateNoteTransformation();
        const double upDec15 = scrap->noteTransformAdjustedDeclination().north;

        const double expected = cwNoteTranformation::northAdjustedForDeclination(upDec0, kDeclination);
        INFO("upDec0=" << upDec0 << " upDec15=" << upDec15 << " expected=" << expected);
        CHECK(upDec15 == Catch::Approx(expected).margin(kToleranceDegrees));
    }
}

TEST_CASE("Guess neighbor station name", "[cwScrap]") {
    class TestRow {
    public:
        TestRow(QString filename) :
            Filename(filename)
        {}

        QString Filename;
    };

    QList<TestRow> rows;
    rows.append(TestRow(testcasesDatasetPath("scrapGuessNeighbor/scrapGuessNeigborPlan.cw")));
    rows.append(TestRow(testcasesDatasetPath("scrapGuessNeighbor/scrapGuessNeigborPlanContinuous.cw")));
    rows.append(TestRow(testcasesDatasetPath("scrapGuessNeighbor/scrapGuessNeigborProfile.cw")));
    rows.append(TestRow(testcasesDatasetPath("scrapGuessNeighbor/scrapGuessNeigborProfileRotate90.cw")));
    rows.append(TestRow(testcasesDatasetPath("scrapGuessNeighbor/scrapGuessNeigborProfileContinuous.cw")));
    rows.append(TestRow(testcasesDatasetPath("scrapAutoCalculate/ProjectProfile-test-v3.cw")));

    foreach(TestRow row, rows) {
        INFO("Testing:" << row.Filename.toStdString());

        auto planProject = fileToProject(row.Filename);
        cwScrap* scrap = firstScrap(planProject.get());

        auto plotManager = std::make_unique<cwLinePlotManager>();
        // Update scrap note transformations when station positions change,
        // mirroring what cwScrapManager does in the full application.
        QObject::connect(plotManager.get(), &cwLinePlotManager::stationPositionInScrapsChanged,
                         [](const QList<cwScrap*>& scraps) {
                             for (cwScrap* s : scraps) {
                                 s->updateNoteTransformation();
                             }
                         });
        plotManager->setRegion(planProject->cavingRegion());
        plotManager->waitToFinish();

        REQUIRE(scrap->stations().size() >= 1);
        cwNoteStation centerStation = scrap->stations().first();

        QList<cwNoteStation> scrapStations = scrap->stations();

        foreach(cwNoteStation noteStation, scrapStations) {
            QSet<cwStation> neighbors = scrap->parentNote()->parentTrip()->neighboringStations(noteStation.name());

            INFO("Center station:" << noteStation.name().toStdString());

            foreach(cwStation neighbor, neighbors) {
                INFO("Neighbor:" << neighbor.name().toStdString());
                auto found = std::find_if(scrapStations.begin(), scrapStations.end(), [neighbor](const cwNoteStation& station) {
                    return station.name().compare(neighbor.name(), Qt::CaseInsensitive) == 0;
                });

                REQUIRE(found != scrapStations.end());

                cwNoteStation neighborNoteStation = *found;

                QString guessedName = scrap->guessNeighborStationName(noteStation, neighborNoteStation.positionOnNote());

                CHECK(neighborNoteStation.name().toLower().toStdString() == guessedName.toStdString());
            }
        }
    }
}

TEST_CASE("Distance lead unit should return the index supported units", "[cwScrap]") {
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    fileToProject(project.get(), testcasesDatasetPath("test_cwScrap/leadLengthCheck.cw"));

    auto trip = project->cavingRegion()->cave(0)->trip(0);
    auto scrap = trip->notes()->notes().at(0)->scrap(0);
    REQUIRE(scrap->leads().size() == 1);

    auto checkUnit = [=](QString unit) {

        bool okay;
        int supportedUnitIndex = scrap->leadData(cwScrap::LeadUnits, 0).toInt(&okay);
        CHECK(okay);
        REQUIRE(supportedUnitIndex >= 0);
        auto supportedUnits = scrap->leadData(cwScrap::LeadSupportedUnits, 0).toStringList();
        REQUIRE(supportedUnitIndex < supportedUnits.size());
        INFO("Supported Units:" << supportedUnits << "unit:" << unit);
        CHECK(supportedUnits.at(supportedUnitIndex).toStdString() == unit.toStdString());
    };

    trip->calibrations()->setDistanceUnit(cwUnits::Meters);
    checkUnit("m");

    trip->calibrations()->setDistanceUnit(cwUnits::Feet);
    checkUnit("ft");
}

TEST_CASE("cwScrap editing depth should toggle editing state", "[cwScrap]") {
    cwScrap scrap;
    cwSignalSpy editingSpy(&scrap, &cwScrap::editingChanged);

    CHECK(scrap.editing() == false);
    CHECK(editingSpy.count() == 0);

    scrap.beginEditing();
    CHECK(scrap.editing() == true);
    REQUIRE(editingSpy.count() == 1);
    CHECK(editingSpy.takeFirst().at(0).toBool() == true);

    scrap.beginEditing();
    CHECK(scrap.editing() == true);
    CHECK(editingSpy.count() == 0);

    scrap.endEditing();
    CHECK(scrap.editing() == true);
    CHECK(editingSpy.count() == 0);

    scrap.endEditing();
    CHECK(scrap.editing() == false);
    REQUIRE(editingSpy.count() == 1);
    CHECK(editingSpy.takeFirst().at(0).toBool() == false);

    {
        cwScrapEditingScope scope(&scrap);
        CHECK(scrap.editing() == true);
    }
    CHECK(scrap.editing() == false);
}
