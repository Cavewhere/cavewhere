// Catch includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

// Our includes
#include "cwImage.h"
#include "cwImageResolution.h"
#include "cwNote.h"
#include "cwNoteMergeApplier.h"
#include "cwNoteMergePlanBuilder.h"
#include "cwSurveyNoteModel.h"

namespace {

cwImage makeImage(const QString& path, int dotsPerMeter)
{
    cwImage image;
    image.setPath(path);
    image.setOriginalSize(QSize(100, 50));
    image.setOriginalDotsPerMeter(dotsPerMeter);
    image.setUnit(cwImage::Unit::Pixels);
    return image;
}

cwNoteData makeLoadedNoteData(const QUuid& id,
                              const QString& name,
                              double rotate,
                              const cwImage& image,
                              const cwImageResolution::Data& resolution)
{
    cwNoteData data;
    data.id = id;
    data.name = name;
    data.rotate = rotate;
    data.image = image;
    data.imageResolution = resolution;
    return data;
}

} // namespace

TEST_CASE("cwNote merge applier merges note bundles independently", "[cwNoteMerge][sync]")
{
    cwNote note;
    const QUuid noteId = note.id();

    note.setName(QStringLiteral("current-name"));
    note.setRotate(10.0);
    note.setImage(makeImage(QStringLiteral("ours.png"), 100));
    note.imageResolution()->setData({cwUnits::DotsPerInch, 100.0, false});

    const cwNoteData loaded = makeLoadedNoteData(
        noteId,
        QStringLiteral("loaded-name"),
        20.0,
        makeImage(QStringLiteral("theirs.png"), 200),
        {cwUnits::DotsPerMeter, 2100.0, false});

    cwNoteData base = makeLoadedNoteData(
        noteId,
        QStringLiteral("base-name"),
        10.0,
        makeImage(QStringLiteral("base.png"), 300),
        {cwUnits::DotsPerInch, 100.0, false});

    cwNoteMergePlan plan;
    plan.currentNote = &note;
    plan.loadedNoteData = &loaded;
    plan.baseNoteData = base;

    REQUIRE_FALSE(cwNoteMergeApplier::applyNoteMergePlan(plan).hasError());

    CHECK(note.name() == QStringLiteral("loaded-name"));
    CHECK(note.rotate() == 20.0); // current matched base -> take loaded
    CHECK(note.image() == makeImage(QStringLiteral("ours.png"), 100)); // conflict -> keep ours
    CHECK(note.imageResolution()->data().unit == cwUnits::DotsPerMeter);
    CHECK(note.imageResolution()->data().value == 2100.0); // current matched base -> take loaded
}

TEST_CASE("cwNote merge applier uses loaded bundles when base is unavailable", "[cwNoteMerge][sync]")
{
    cwNote note;
    const QUuid noteId = note.id();

    note.setRotate(5.0);
    note.setImage(makeImage(QStringLiteral("current.png"), 100));
    note.imageResolution()->setData({cwUnits::DotsPerInch, 72.0, false});

    const cwNoteData loaded = makeLoadedNoteData(
        noteId,
        QStringLiteral("loaded"),
        15.0,
        makeImage(QStringLiteral("loaded.png"), 400),
        {cwUnits::DotsPerMeter, 2400.0, false});

    cwNoteMergePlan plan;
    plan.currentNote = &note;
    plan.loadedNoteData = &loaded;

    REQUIRE_FALSE(cwNoteMergeApplier::applyNoteMergePlan(plan).hasError());
    CHECK(note.rotate() == 15.0);
    CHECK(note.image() == makeImage(QStringLiteral("loaded.png"), 400));
    CHECK(note.imageResolution()->data().unit == cwUnits::DotsPerMeter);
    CHECK(note.imageResolution()->data().value == 2400.0);
}

TEST_CASE("cwNote merge plan builder rejects ambiguous loaded note ids", "[cwNoteMerge][sync]")
{
    cwSurveyNoteModel model;
    auto* first = new cwNote(&model);
    auto* second = new cwNote(&model);
    model.addNotes({first, second});

    cwSurveyNoteModelData loaded = model.data();
    REQUIRE(loaded.notes.size() == 2);
    loaded.notes[1].id = loaded.notes[0].id;

    const auto preparation = cwNoteMergePlanBuilder::build(&model, loaded, {});
    CHECK(preparation.hasError());
    CHECK(preparation.errorMessage() == QStringLiteral("Ambiguous loaded note ids."));
}
