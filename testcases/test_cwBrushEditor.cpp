/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwBrushEditor.h"
#include "cwBrushStructureModel.h"
#include "cwLineBrush.h"
#include "cwSymbologyPalette.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteSeed.h"
// cwBrushEditor holds QPointer members of these (and cwSketchCanvas has inline
// accessors over forward-declared cwScrapManager/cwTrip), so a TU that
// instantiates the editor by value needs the complete types.
#include "cwSketchCanvas.h"
#include "cwCavingRegion.h"
#include "cwScrapManager.h"
#include "cwTrip.h"

// Qt
#include <QSignalSpy>

namespace {

// A writable palette seeded from the shipped default, so the editor loads a
// realistic brush (floor-step: a Trace edge layer + a tick-stamp layer).
void seedWritable(cwSymbologyPalette &palette)
{
    palette.setData(cwSymbologyPaletteSeed::create());
    palette.setWritable(true);
}

// floor-step's tick layer (index 1) and its first rule (Uniform spacing).
constexpr int kTickLayer = 1;
constexpr int kFirstTickRule = 0;

} // namespace

TEST_CASE("loadBrushNamed loads a clean working copy with readable structure",
          "[cwBrushEditor]")
{
    cwSymbologyPalette palette;
    seedWritable(palette);

    cwBrushEditor editor;
    editor.setPalette(&palette);
    editor.loadBrushNamed(cwSymbologyPaletteSeed::floorStepBrushName());

    CHECK(editor.isLoaded());
    CHECK_FALSE(editor.isDirty());
    CHECK(editor.brushName() == cwSymbologyPaletteSeed::floorStepBrushName());

    // floor-step: an edge (Trace) layer plus a tick-stamp layer.
    REQUIRE(editor.layerCount() == 2);
    CHECK(editor.layerLabel(0).isEmpty());     // line layer names no glyph
    CHECK(editor.layerLabel(kTickLayer) == cwSymbologyPaletteSeed::floorStepTickGlyphName());
    CHECK(editor.ruleCount(0) == 1);
    CHECK(editor.ruleName(0, 0) == QStringLiteral("Trace"));
    CHECK(editor.ruleCount(kTickLayer) == 3);
    CHECK(editor.ruleName(kTickLayer, kFirstTickRule) == QStringLiteral("Uniform spacing"));
    CHECK(editor.ruleEnabled(kTickLayer, kFirstTickRule));
}

TEST_CASE("loadBrushNamed with an unknown name unloads the editor",
          "[cwBrushEditor]")
{
    cwSymbologyPalette palette;
    seedWritable(palette);

    cwBrushEditor editor;
    editor.setPalette(&palette);
    editor.loadBrushNamed(QStringLiteral("not-a-brush"));

    CHECK_FALSE(editor.isLoaded());
    CHECK(editor.layerCount() == 0);
}

TEST_CASE("setRuleEnabled edits the working copy only, marking it dirty",
          "[cwBrushEditor]")
{
    cwSymbologyPalette palette;
    seedWritable(palette);

    cwBrushEditor editor;
    editor.setPalette(&palette);
    editor.loadBrushNamed(cwSymbologyPaletteSeed::floorStepBrushName());

    QSignalSpy dirtySpy(&editor, &cwBrushEditor::dirtyChanged);
    // The structure model is authoritative for the view: a toggle is a narrow
    // dataChanged on the touched rule (not a tree-collapsing reset).
    QSignalSpy dataChangedSpy(editor.structureModel(), &QAbstractItemModel::dataChanged);
    QSignalSpy paletteBrushSpy(&palette, &cwSymbologyPalette::brushChanged);

    editor.setRuleEnabled(kTickLayer, kFirstTickRule, false);

    CHECK(editor.isDirty());
    CHECK_FALSE(editor.ruleEnabled(kTickLayer, kFirstTickRule));
    CHECK(dirtySpy.count() == 1);
    CHECK(dataChangedSpy.count() == 1);

    // The palette is untouched until apply().
    CHECK(paletteBrushSpy.count() == 0);
    const auto stored = palette.brush(cwSymbologyPaletteSeed::floorStepBrushName());
    REQUIRE(stored.has_value());
    CHECK(stored->decorations.at(kTickLayer).rules.at(kFirstTickRule).enabled);

    // An out-of-range toggle is ignored.
    editor.setRuleEnabled(99, 0, false);
    CHECK(dataChangedSpy.count() == 1);
}

TEST_CASE("discard restores the baseline working copy",
          "[cwBrushEditor]")
{
    cwSymbologyPalette palette;
    seedWritable(palette);

    cwBrushEditor editor;
    editor.setPalette(&palette);
    editor.loadBrushNamed(cwSymbologyPaletteSeed::floorStepBrushName());

    editor.setRuleEnabled(kTickLayer, kFirstTickRule, false);
    REQUIRE(editor.isDirty());

    editor.discard();

    CHECK_FALSE(editor.isDirty());
    CHECK(editor.ruleEnabled(kTickLayer, kFirstTickRule));
}

TEST_CASE("apply on a writable palette commits the edit and clears dirty",
          "[cwBrushEditor]")
{
    cwSymbologyPalette palette;
    seedWritable(palette);

    cwBrushEditor editor;
    editor.setPalette(&palette);
    editor.loadBrushNamed(cwSymbologyPaletteSeed::floorStepBrushName());

    editor.setRuleEnabled(kTickLayer, kFirstTickRule, false);
    REQUIRE(editor.isDirty());

    QSignalSpy paletteBrushSpy(&palette, &cwSymbologyPalette::brushChanged);
    editor.apply();

    CHECK_FALSE(editor.isDirty());
    REQUIRE(paletteBrushSpy.count() == 1);
    CHECK(paletteBrushSpy.takeFirst().at(0).toString()
          == cwSymbologyPaletteSeed::floorStepBrushName());

    const auto stored = palette.brush(cwSymbologyPaletteSeed::floorStepBrushName());
    REQUIRE(stored.has_value());
    CHECK_FALSE(stored->decorations.at(kTickLayer).rules.at(kFirstTickRule).enabled);
}

TEST_CASE("addRule appends a rule to a layer, dirtying it and inserting one row",
          "[cwBrushEditor]")
{
    cwSymbologyPalette palette;
    seedWritable(palette);

    cwBrushEditor editor;
    editor.setPalette(&palette);
    editor.loadBrushNamed(cwSymbologyPaletteSeed::floorStepBrushName());

    QSignalSpy insertSpy(editor.structureModel(), &QAbstractItemModel::rowsInserted);

    const int before = editor.ruleCount(kTickLayer);
    const QString newRule = QStringLiteral("Bending stamp");   // any registered rule name

    editor.addRule(kTickLayer, newRule);

    CHECK(editor.ruleCount(kTickLayer) == before + 1);
    CHECK(editor.ruleName(kTickLayer, before) == newRule);   // appended at the end
    CHECK(editor.isDirty());
    CHECK(insertSpy.count() == 1);

    // Discard removes the added rule.
    editor.discard();
    CHECK(editor.ruleCount(kTickLayer) == before);
    CHECK_FALSE(editor.isDirty());

    // An out-of-range layer is ignored.
    editor.addRule(99, newRule);
    CHECK(insertSpy.count() == 1);
}

TEST_CASE("removeRule deletes a rule from a layer, dirtying it and removing one row",
          "[cwBrushEditor]")
{
    cwSymbologyPalette palette;
    seedWritable(palette);

    cwBrushEditor editor;
    editor.setPalette(&palette);
    editor.loadBrushNamed(cwSymbologyPaletteSeed::floorStepBrushName());

    QSignalSpy removeSpy(editor.structureModel(), &QAbstractItemModel::rowsRemoved);

    const int before = editor.ruleCount(kTickLayer);
    const QString secondRule = editor.ruleName(kTickLayer, kFirstTickRule + 1);

    editor.removeRule(kTickLayer, kFirstTickRule);

    CHECK(editor.ruleCount(kTickLayer) == before - 1);
    CHECK(editor.ruleName(kTickLayer, kFirstTickRule) == secondRule);   // successor shifts down
    CHECK(editor.isDirty());
    CHECK(removeSpy.count() == 1);

    // An out-of-range rule is ignored.
    editor.removeRule(kTickLayer, 99);
    CHECK(removeSpy.count() == 1);

    editor.discard();
    CHECK(editor.ruleCount(kTickLayer) == before);
    CHECK_FALSE(editor.isDirty());
}

TEST_CASE("apply on a read-only palette without a project leaves it untouched",
          "[cwBrushEditor]")
{
    cwSymbologyPalette palette;
    seedWritable(palette);

    cwBrushEditor editor;
    editor.setPalette(&palette);
    editor.loadBrushNamed(cwSymbologyPaletteSeed::floorStepBrushName());

    // The user edits freely on the working copy regardless of writability.
    editor.setRuleEnabled(kTickLayer, kFirstTickRule, false);
    palette.setWritable(false);

    QSignalSpy paletteBrushSpy(&palette, &cwSymbologyPalette::brushChanged);
    editor.apply();   // no manager/project here, so the fork cannot materialize

    // The read-only palette is never mutated; fork-on-save is exercised in the
    // QML test where a real manager + project exist.
    CHECK(paletteBrushSpy.count() == 0);
    const auto stored = palette.brush(cwSymbologyPaletteSeed::floorStepBrushName());
    REQUIRE(stored.has_value());
    CHECK(stored->decorations.at(kTickLayer).rules.at(kFirstTickRule).enabled);
}
