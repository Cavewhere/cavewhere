// Catch includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

// Our includes
#include "cwNoteLiDAR.h"
#include "cwNoteLiDARMergeApplier.h"
#include "cwNoteLiDARMergePlanBuilder.h"
#include "cwSurveyNoteLiDARModel.h"

#include <memory>

namespace {

cwNoteLiDARStation makeStation(const QUuid& id, const QString& name, const QVector3D& position)
{
    cwNoteLiDARStation station;
    station.setId(id);
    station.setName(name);
    station.setPositionOnNote(position);
    return station;
}

cwNoteLiDARData makeLiDARData(const QUuid& noteId,
                              const QString& filename,
                              const QList<cwNoteLiDARStation>& stations)
{
    cwNoteLiDARData data;
    data.id = noteId;
    data.name = QStringLiteral("LiDAR Note");
    data.filename = filename;
    data.stations = stations;
    data.autoCalculateNorth = false;
    data.transfrom.north = 0.0;
    return data;
}

} // namespace

TEST_CASE("cwNoteLiDAR merge plan builder accepts note reorder with stable identities", "[cwNoteLiDARMerge][sync]")
{
    cwSurveyNoteLiDARModel model;

    auto* firstNote = new cwNoteLiDAR(&model);
    auto* secondNote = new cwNoteLiDAR(&model);
    model.addNotes({firstNote, secondNote});

    cwSurveyNoteLiDARModelData loadedData;
    loadedData.notes = {
        secondNote->data(),
        firstNote->data()
    };

    const auto applyMode = cwNoteLiDARMergePlanBuilder::determineApplyMode(&model, loadedData);
    CHECK(applyMode == cwNoteLiDARDescriptorApplyMode::IncrementalMerge);

    const auto preparation = cwNoteLiDARMergePlanBuilder::build(&model, loadedData, {});
    REQUIRE_FALSE(preparation.hasError());
    REQUIRE(preparation.value().orderedNotes.size() == 2);
    CHECK(preparation.value().orderedNotes.at(0) == secondNote);
    CHECK(preparation.value().orderedNotes.at(1) == firstNote);
    REQUIRE(preparation.value().plans.size() == 2);
}

TEST_CASE("cwNoteLiDAR merge plan builder rejects ambiguous note identities", "[cwNoteLiDARMerge][sync]")
{
    cwSurveyNoteLiDARModel model;

    auto* firstNote = new cwNoteLiDAR(&model);
    auto* secondNote = new cwNoteLiDAR(&model);
    model.addNotes({firstNote, secondNote});

    SECTION("duplicate loaded note ids")
    {
        cwSurveyNoteLiDARModelData loadedData = model.data();
        REQUIRE(loadedData.notes.size() == 2);
        loadedData.notes[1].id = loadedData.notes[0].id;

        const auto applyMode = cwNoteLiDARMergePlanBuilder::determineApplyMode(&model, loadedData);
        CHECK(applyMode == cwNoteLiDARDescriptorApplyMode::Ambiguous);
    }

    SECTION("null loaded note id")
    {
        cwSurveyNoteLiDARModelData loadedData = model.data();
        REQUIRE(loadedData.notes.size() == 2);
        loadedData.notes[1].id = QUuid();

        const auto applyMode = cwNoteLiDARMergePlanBuilder::determineApplyMode(&model, loadedData);
        CHECK(applyMode == cwNoteLiDARDescriptorApplyMode::Ambiguous);
    }

    SECTION("duplicate current note ids")
    {
        secondNote->setId(firstNote->id());
        cwSurveyNoteLiDARModelData loadedData = model.data();

        const auto applyMode = cwNoteLiDARMergePlanBuilder::determineApplyMode(&model, loadedData);
        CHECK(applyMode == cwNoteLiDARDescriptorApplyMode::Ambiguous);
    }
}

TEST_CASE("cwNoteLiDAR merge applier merges stations by stable id with local precedence", "[cwNoteLiDARMerge][sync]")
{
    auto note = std::make_unique<cwNoteLiDAR>();
    note->setFilename(QStringLiteral("ours.glb"));

    const QUuid sharedStationId = QUuid::createUuid();
    const QUuid localOnlyStationId = QUuid::createUuid();
    const QUuid remoteOnlyStationId = QUuid::createUuid();

    note->setStations({
        makeStation(sharedStationId, QStringLiteral("ours-shared"), QVector3D(1.0f, 1.0f, 1.0f)),
        makeStation(localOnlyStationId, QStringLiteral("ours-local"), QVector3D(2.0f, 2.0f, 2.0f))
    });

    const QUuid noteId = note->id();
    cwNoteLiDARData loadedData = makeLiDARData(
        noteId,
        QStringLiteral("theirs.glb"),
        {
            makeStation(sharedStationId, QStringLiteral("theirs-shared"), QVector3D(10.0f, 10.0f, 10.0f)),
            makeStation(remoteOnlyStationId, QStringLiteral("theirs-remote"), QVector3D(20.0f, 20.0f, 20.0f))
        });

    cwNoteLiDARData baseData = makeLiDARData(
        noteId,
        QStringLiteral("ours.glb"),
        {
            makeStation(sharedStationId, QStringLiteral("base-shared"), QVector3D(0.5f, 0.5f, 0.5f))
        });

    cwNoteLiDARMergePlan plan;
    plan.currentNote = note.get();
    plan.loadedNoteData = &loadedData;
    plan.baseNoteData = baseData;

    REQUIRE_FALSE(cwNoteLiDARMergeApplier::applyNoteLiDARMergePlan(plan).hasError());

    CHECK(note->filename() == QStringLiteral("theirs.glb"));
    const QList<cwNoteLiDARStation> mergedStations = note->stations();
    REQUIRE(mergedStations.size() == 3);

    auto findStationById = [&](const QUuid& id) -> std::optional<cwNoteLiDARStation> {
        for (const cwNoteLiDARStation& station : mergedStations) {
            if (station.id() == id) {
                return station;
            }
        }
        return std::nullopt;
    };

    const auto sharedStation = findStationById(sharedStationId);
    const auto localOnlyStation = findStationById(localOnlyStationId);
    const auto remoteOnlyStation = findStationById(remoteOnlyStationId);
    REQUIRE(sharedStation.has_value());
    REQUIRE(localOnlyStation.has_value());
    REQUIRE(remoteOnlyStation.has_value());

    CHECK(sharedStation->name() == QStringLiteral("ours-shared"));
    CHECK(localOnlyStation->name() == QStringLiteral("ours-local"));
    CHECK(remoteOnlyStation->name() == QStringLiteral("theirs-remote"));
}

TEST_CASE("cwNoteLiDAR merge applier applies remote station value when local matches base", "[cwNoteLiDARMerge][sync]")
{
    auto note = std::make_unique<cwNoteLiDAR>();

    const QUuid stationId = QUuid::createUuid();
    const QUuid noteId = note->id();
    const cwNoteLiDARStation baseStation = makeStation(stationId, QStringLiteral("S0"), QVector3D(0.1f, 0.2f, 0.3f));

    note->setStations({baseStation});

    cwNoteLiDARData loadedData = makeLiDARData(
        noteId,
        QStringLiteral("remote.glb"),
        {makeStation(stationId, QStringLiteral("S0-remote"), QVector3D(9.0f, 8.0f, 7.0f))});
    cwNoteLiDARData baseData = makeLiDARData(noteId,
                                             QStringLiteral("local.glb"),
                                             {baseStation});

    cwNoteLiDARMergePlan plan;
    plan.currentNote = note.get();
    plan.loadedNoteData = &loadedData;
    plan.baseNoteData = baseData;

    REQUIRE_FALSE(cwNoteLiDARMergeApplier::applyNoteLiDARMergePlan(plan).hasError());

    const QList<cwNoteLiDARStation> mergedStations = note->stations();
    REQUIRE(mergedStations.size() == 1);
    CHECK(mergedStations[0].id() == stationId);
    CHECK(mergedStations[0].name() == QStringLiteral("S0-remote"));
    CHECK(mergedStations[0].positionOnNote() == QVector3D(9.0f, 8.0f, 7.0f));
}

TEST_CASE("cwNoteLiDAR merge applier returns false for ambiguous station ids", "[cwNoteLiDARMerge][sync]")
{
    auto note = std::make_unique<cwNoteLiDAR>();

    const QUuid sharedId = QUuid::createUuid();
    const QUuid noteId = note->id();
    note->setStations({
        makeStation(sharedId, QStringLiteral("ours"), QVector3D(1.0f, 1.0f, 1.0f))
    });

    cwNoteLiDARData loadedData = makeLiDARData(
        noteId,
        QStringLiteral("remote.glb"),
        {
            makeStation(sharedId, QStringLiteral("theirs-a"), QVector3D(2.0f, 2.0f, 2.0f)),
            makeStation(sharedId, QStringLiteral("theirs-b"), QVector3D(3.0f, 3.0f, 3.0f))
        });
    cwNoteLiDARData baseData = makeLiDARData(
        noteId,
        QStringLiteral("base.glb"),
        {makeStation(sharedId, QStringLiteral("base"), QVector3D(1.0f, 1.0f, 1.0f))});

    cwNoteLiDARMergePlan plan;
    plan.currentNote = note.get();
    plan.loadedNoteData = &loadedData;
    plan.baseNoteData = baseData;

    CHECK(cwNoteLiDARMergeApplier::applyNoteLiDARMergePlan(plan).hasError());
}
