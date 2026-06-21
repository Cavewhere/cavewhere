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

TEST_CASE("loadBrushNamed normalizes stored rule order to pipeline stage order",
          "[cwBrushEditor]")
{
    cwSymbologyPalette palette;
    seedWritable(palette);

    // A brush whose rules are stored out of stage order. The layout stage-sorts
    // at render time; the editor mirrors that so the displayed order is the order
    // that actually runs.
    cwLineBrush brush;
    brush.name = QStringLiteral("scrambled");
    brush.displayName = QStringLiteral("Scrambled");
    cwDecorationLayer layer;
    layer.rules = {
        cwPlacementRuleData{QStringLiteral("Rigid stamp"), {}, true},      // Terminal
        cwPlacementRuleData{QStringLiteral("Uniform spacing"), {}, true},  // Generate
        cwPlacementRuleData{QStringLiteral("Align to tangent"), {}, true}, // MutatePerLayer
    };
    brush.decorations.append(layer);
    palette.setBrush(brush);

    cwBrushEditor editor;
    editor.setPalette(&palette);
    editor.loadBrushNamed(QStringLiteral("scrambled"));

    REQUIRE(editor.ruleCount(0) == 3);
    CHECK(editor.ruleName(0, 0) == QStringLiteral("Uniform spacing"));
    CHECK(editor.ruleName(0, 1) == QStringLiteral("Align to tangent"));
    CHECK(editor.ruleName(0, 2) == QStringLiteral("Rigid stamp"));
    CHECK_FALSE(editor.isDirty());   // reordering on load is not an edit
}

TEST_CASE("addRule inserts a rule into its stage block, keeping pipeline order",
          "[cwBrushEditor]")
{
    cwSymbologyPalette palette;
    seedWritable(palette);

    cwBrushEditor editor;
    editor.setPalette(&palette);
    editor.loadBrushNamed(cwSymbologyPaletteSeed::floorStepBrushName());

    // tick layer: Uniform spacing (Placement) -> Align to tangent (Orientation)
    //             -> Rigid stamp (Output).
    // A Stroke-stage rule (Offset stroke) sorts ahead of all three.
    editor.addRule(kTickLayer, QStringLiteral("Offset stroke"));
    CHECK(editor.ruleName(kTickLayer, 0) == QStringLiteral("Offset stroke"));

    // A second Placement rule lands at the end of the Placement block — after
    // Uniform spacing, before the Orientation rule.
    editor.addRule(kTickLayer, QStringLiteral("Explicit point"));
    CHECK(editor.ruleName(kTickLayer, 1) == QStringLiteral("Uniform spacing"));
    CHECK(editor.ruleName(kTickLayer, 2) == QStringLiteral("Explicit point"));
    CHECK(editor.ruleName(kTickLayer, 3) == QStringLiteral("Align to tangent"));
}

TEST_CASE("structure model nests rules under per-stage category nodes",
          "[cwBrushEditor]")
{
    cwSymbologyPalette palette;
    seedWritable(palette);

    cwBrushEditor editor;
    editor.setPalette(&palette);
    editor.loadBrushNamed(cwSymbologyPaletteSeed::floorStepBrushName());

    cwBrushStructureModel *model = editor.structureModel();
    const QModelIndex tickLayer = model->index(kTickLayer, 0);

    // Each tick-layer rule is the sole member of its stage, so the layer holds
    // three category nodes (Placement -> Orientation -> Output), each with one
    // rule child.
    REQUIRE(model->rowCount(tickLayer) == 3);

    const QModelIndex placement = model->index(0, 0, tickLayer);
    CHECK(model->data(placement, cwBrushStructureModel::IsCategoryRole).toBool());
    CHECK(model->data(placement, cwBrushStructureModel::CategoryRole).toString()
          == QStringLiteral("Placement"));
    CHECK(model->data(placement, cwBrushStructureModel::CategorySizeRole).toInt() == 1);
    REQUIRE(model->rowCount(placement) == 1);

    // The rule sits under its category node; its parent round-trips back to it,
    // and RuleIndexRole reports the flat index the editor's mutators use.
    const QModelIndex rule0 = model->index(0, 0, placement);
    CHECK_FALSE(model->data(rule0, cwBrushStructureModel::IsCategoryRole).toBool());
    CHECK(model->data(rule0, cwBrushStructureModel::LabelRole).toString()
          == QStringLiteral("Uniform spacing"));
    CHECK(model->data(rule0, cwBrushStructureModel::CategoryRole).toString()
          == QStringLiteral("Placement"));
    CHECK(model->data(rule0, cwBrushStructureModel::RuleIndexRole).toInt() == 0);
    CHECK(model->parent(rule0) == placement);
    CHECK(model->layerIndexOf(rule0) == kTickLayer);
    CHECK(model->ruleIndexOf(rule0) == 0);

    const QModelIndex orientation = model->index(1, 0, tickLayer);
    CHECK(model->data(orientation, cwBrushStructureModel::CategoryRole).toString()
          == QStringLiteral("Orientation"));

    // The Output rule's flat index survives the nesting (it is the third stored
    // rule even though it is the first child of the Output category).
    const QModelIndex output = model->index(2, 0, tickLayer);
    const QModelIndex outputRule = model->index(0, 0, output);
    CHECK(model->data(outputRule, cwBrushStructureModel::RuleIndexRole).toInt() == 2);

    // A layer row carries no category.
    CHECK(model->data(tickLayer, cwBrushStructureModel::IsLayerRole).toBool());
    CHECK(model->data(tickLayer, cwBrushStructureModel::CategoryRole).toString().isEmpty());
    CHECK(model->data(tickLayer, cwBrushStructureModel::CategorySizeRole).toInt() == 0);

    // Adding a second Placement rule grows the Placement category to two children
    // (the drag-handle gate) without adding a category node, and refreshes the
    // sibling's positional role without a reset.
    QSignalSpy dataChangedSpy(model, &QAbstractItemModel::dataChanged);
    QSignalSpy insertSpy(model, &QAbstractItemModel::rowsInserted);
    editor.addRule(kTickLayer, QStringLiteral("Explicit point"));

    CHECK(model->rowCount(tickLayer) == 3);                 // still three categories
    const QModelIndex placement2 = model->index(0, 0, tickLayer);
    REQUIRE(model->rowCount(placement2) == 2);
    CHECK(model->data(model->index(0, 0, placement2),
                      cwBrushStructureModel::CategorySizeRole).toInt() == 2);
    CHECK(insertSpy.count() == 1);
    CHECK(dataChangedSpy.count() >= 1);
}

TEST_CASE("adding a rule of a new stage inserts a category node",
          "[cwBrushEditor]")
{
    cwSymbologyPalette palette;
    seedWritable(palette);

    cwBrushEditor editor;
    editor.setPalette(&palette);
    editor.loadBrushNamed(cwSymbologyPaletteSeed::floorStepBrushName());

    cwBrushStructureModel *model = editor.structureModel();
    const QModelIndex tickLayer = model->index(kTickLayer, 0);
    REQUIRE(model->rowCount(tickLayer) == 3);   // Placement, Orientation, Output

    // A Stroke-stage rule is a stage the tick layer doesn't have yet, so a new
    // category node appears at the front (Stroke sorts before every other stage).
    QSignalSpy insertSpy(model, &QAbstractItemModel::rowsInserted);
    editor.addRule(kTickLayer, QStringLiteral("Offset stroke"));

    REQUIRE(model->rowCount(tickLayer) == 4);
    const QModelIndex stroke = model->index(0, 0, tickLayer);
    CHECK(model->data(stroke, cwBrushStructureModel::CategoryRole).toString()
          == QStringLiteral("Stroke"));
    REQUIRE(model->rowCount(stroke) == 1);
    CHECK(model->data(model->index(0, 0, stroke),
                      cwBrushStructureModel::LabelRole).toString()
          == QStringLiteral("Offset stroke"));
    CHECK(insertSpy.count() == 1);

    // Removing it again drops the whole Stroke category node.
    QSignalSpy removeSpy(model, &QAbstractItemModel::rowsRemoved);
    editor.removeRule(kTickLayer, 0);
    CHECK(model->rowCount(tickLayer) == 3);
    CHECK(removeSpy.count() == 1);
}

TEST_CASE("moveRule reorders rules within a category but rejects cross-category moves",
          "[cwBrushEditor]")
{
    cwSymbologyPalette palette;
    seedWritable(palette);

    cwBrushEditor editor;
    editor.setPalette(&palette);
    editor.loadBrushNamed(cwSymbologyPaletteSeed::floorStepBrushName());

    // Give the Placement block two rules so an in-category swap is possible:
    // Uniform spacing (0) + Explicit point (1), then Align to tangent (2,
    // Orientation) and Rigid stamp (3, Output).
    editor.addRule(kTickLayer, QStringLiteral("Explicit point"));
    REQUIRE(editor.ruleCount(kTickLayer) == 4);
    REQUIRE(editor.ruleName(kTickLayer, 0) == QStringLiteral("Uniform spacing"));
    REQUIRE(editor.ruleName(kTickLayer, 1) == QStringLiteral("Explicit point"));
    editor.discard();   // back to the clean 3-rule baseline for the move tests
    editor.addRule(kTickLayer, QStringLiteral("Explicit point"));

    // A within-category reorder is a single rowsMoved on the shared category node.
    QSignalSpy moveSpy(editor.structureModel(), &QAbstractItemModel::rowsMoved);

    // Swap the two Placement rules (same category) — allowed.
    editor.moveRule(kTickLayer, 0, 1);
    CHECK(editor.ruleName(kTickLayer, 0) == QStringLiteral("Explicit point"));
    CHECK(editor.ruleName(kTickLayer, 1) == QStringLiteral("Uniform spacing"));
    CHECK(editor.isDirty());
    CHECK(moveSpy.count() == 1);

    // A move that would cross a category boundary (Placement -> Output) is
    // rejected, so the stored order stays stage-grouped.
    editor.moveRule(kTickLayer, 0, 3);
    CHECK(editor.ruleName(kTickLayer, 0) == QStringLiteral("Explicit point"));
    CHECK(editor.ruleName(kTickLayer, 3) == QStringLiteral("Rigid stamp"));
    CHECK(moveSpy.count() == 1);

    // No-op and out-of-range moves are ignored too.
    editor.moveRule(kTickLayer, 1, 1);
    editor.moveRule(kTickLayer, 0, 99);
    editor.moveRule(99, 0, 1);
    CHECK(moveSpy.count() == 1);
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
