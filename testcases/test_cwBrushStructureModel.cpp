/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwBrushStructureModel.h"
#include "cwDecorationLayer.h"
#include "cwLineBrush.h"
#include "cwPlacementRuleData.h"

// Qt
#include <QAbstractItemModel>
#include <QPersistentModelIndex>
#include <QSignalSpy>

// These cases pin the stable-node-id contract: layer add/remove/move emit proper
// row signals (never a model reset), and a QPersistentModelIndex into a layer's
// subtree keeps resolving to the same rule across structural layer edits.

namespace {

// A decoration layer carrying the given (registry-known) rules in authored order
// and an identifying glyph name.
cwDecorationLayer makeLayer(const QString &glyphName, const QStringList &ruleNames)
{
    cwDecorationLayer layer;
    layer.glyphName = glyphName;
    for (const QString &name : ruleNames) {
        cwPlacementRuleData ruleData;
        ruleData.name = name;
        layer.rules.append(ruleData);
    }
    return layer;
}

// A three-layer brush; layer 2 carries two stage blocks (a category each) so its
// subtree has category and rule nodes to hold persistent indexes against.
cwLineBrush makeThreeLayerBrush()
{
    cwLineBrush brush;
    brush.decorations.append(makeLayer(QStringLiteral("alpha"), {QStringLiteral("Uniform spacing")}));
    brush.decorations.append(makeLayer(QStringLiteral("beta"),  {QStringLiteral("Uniform spacing")}));
    brush.decorations.append(makeLayer(QStringLiteral("gamma"),
                                       {QStringLiteral("Uniform spacing"),
                                        QStringLiteral("Rotate by tangent")}));
    return brush;
}

// The rule row whose flat rule index is flatRuleIndex under layerIndex.
QModelIndex findRuleRow(const cwBrushStructureModel &model, int layerIndex, int flatRuleIndex)
{
    const QModelIndex layer = model.index(layerIndex, 0);
    for (int c = 0; c < model.rowCount(layer); ++c) {
        const QModelIndex category = model.index(c, 0, layer);
        for (int r = 0; r < model.rowCount(category); ++r) {
            const QModelIndex rule = model.index(r, 0, category);
            if (model.ruleIndexOf(rule) == flatRuleIndex) {
                return rule;
            }
        }
    }
    return {};
}

} // namespace

TEST_CASE("Layer ops emit row signals, never a model reset", "[cwBrushStructureModel]")
{
    cwBrushStructureModel model;
    model.setBrush(makeThreeLayerBrush());

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy moveSpy(&model, &QAbstractItemModel::rowsMoved);

    model.insertLayer(0, makeLayer(QStringLiteral("delta"), {QStringLiteral("Uniform spacing")}));
    CHECK(model.layerCount() == 4);
    CHECK(insertSpy.count() == 1);

    model.moveLayer(0, 3);
    CHECK(moveSpy.count() == 1);

    model.removeLayer(3);
    CHECK(model.layerCount() == 3);
    CHECK(removeSpy.count() == 1);

    // No structural edit ever fell back to a reset.
    CHECK(resetSpy.count() == 0);
}

TEST_CASE("layerCountChanged fires for add/remove and count-changing loads, not moves",
          "[cwBrushStructureModel]")
{
    cwBrushStructureModel model;
    QSignalSpy countSpy(&model, &cwBrushStructureModel::layerCountChanged);

    model.setBrush(makeThreeLayerBrush());   // 0 -> 3 layers
    CHECK(model.layerCount() == 3);
    CHECK(countSpy.count() == 1);

    model.insertLayer(3, makeLayer(QStringLiteral("delta"), {QStringLiteral("Uniform spacing")}));
    CHECK(countSpy.count() == 2);

    model.removeLayer(3);
    CHECK(countSpy.count() == 3);

    model.moveLayer(0, 2);   // count unchanged
    CHECK(countSpy.count() == 3);

    cwLineBrush oneLayer;
    oneLayer.decorations.append(makeLayer(QStringLiteral("solo"), {QStringLiteral("Uniform spacing")}));
    model.setBrush(oneLayer);   // 3 -> 1 layers
    CHECK(countSpy.count() == 4);

    model.setBrush(oneLayer);   // 1 -> 1 layers: no change
    CHECK(countSpy.count() == 4);
}

TEST_CASE("Persistent indexes into a layer's subtree survive layer edits",
          "[cwBrushStructureModel]")
{
    cwBrushStructureModel model;
    model.setBrush(makeThreeLayerBrush());

    // Hold a category and a rule under layer 2 (glyph "gamma", second rule
    // "Rotate by tangent").
    const QModelIndex layer2 = model.index(2, 0);
    const QPersistentModelIndex category(model.index(model.rowCount(layer2) - 1, 0, layer2));
    const QPersistentModelIndex rule(findRuleRow(model, 2, 1));
    REQUIRE(category.isValid());
    REQUIRE(rule.isValid());
    REQUIRE(model.data(rule, cwBrushStructureModel::LabelRole).toString()
            == QStringLiteral("Rotate by tangent"));

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    // Insert at the front: gamma slides from row 2 to row 3.
    model.insertLayer(0, makeLayer(QStringLiteral("delta"), {QStringLiteral("Uniform spacing")}));
    CHECK(rule.isValid());
    CHECK(model.layerIndexOf(rule) == 3);
    CHECK(model.layerIndexOf(category) == 3);
    CHECK(model.data(rule, cwBrushStructureModel::LabelRole).toString()
          == QStringLiteral("Rotate by tangent"));

    // Move the front layer to the back: gamma slides from row 3 to row 2.
    model.moveLayer(0, 3);
    CHECK(rule.isValid());
    CHECK(model.layerIndexOf(rule) == 2);
    CHECK(model.data(rule, cwBrushStructureModel::LabelRole).toString()
          == QStringLiteral("Rotate by tangent"));

    // Remove the front layer: gamma slides from row 2 to row 1.
    model.removeLayer(0);
    CHECK(rule.isValid());
    CHECK(model.layerIndexOf(rule) == 1);
    CHECK(model.layerIndexOf(category) == 1);
    CHECK(model.data(rule, cwBrushStructureModel::LabelRole).toString()
          == QStringLiteral("Rotate by tangent"));

    CHECK(resetSpy.count() == 0);
}

TEST_CASE("A rule index survives a new category block inserted above it",
          "[cwBrushStructureModel]")
{
    // The latent stranding Phase B closes: a category is keyed by its pipeline
    // stage, not its row, so a new category block appearing above a held rule
    // shifts that rule's category position without invalidating the rule's index.
    cwBrushStructureModel model;
    cwLineBrush brush;
    // One layer whose only rule is a Terminal-stage rule ("Rigid stamp"), so it
    // sits in the single (last) category block.
    brush.decorations.append(makeLayer(QStringLiteral("tick"), {QStringLiteral("Rigid stamp")}));
    model.setBrush(brush);

    const QPersistentModelIndex rule(findRuleRow(model, 0, 0));
    REQUIRE(rule.isValid());
    REQUIRE(model.data(rule, cwBrushStructureModel::LabelRole).toString()
            == QStringLiteral("Rigid stamp"));
    REQUIRE(model.parent(rule).row() == 0);   // first (only) category block

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    // Insert a Generate-stage rule at flat index 0: a brand-new category block
    // appears ABOVE the Terminal block, sliding it from category row 0 to 1.
    cwPlacementRuleData generate;
    generate.name = QStringLiteral("Uniform spacing");
    REQUIRE(model.insertRule(0, 0, generate));

    CHECK(rule.isValid());
    // Still the same rule: "Rigid stamp" is now flat index 1 (Uniform spacing took
    // index 0), and its category parent tracked the shift down to row 1 rather than
    // stranding at the new block in row 0.
    CHECK(model.data(rule, cwBrushStructureModel::LabelRole).toString()
          == QStringLiteral("Rigid stamp"));
    CHECK(model.ruleIndexOf(rule) == 1);
    CHECK(model.layerIndexOf(rule) == 0);
    CHECK(model.parent(rule).row() == 1);

    // A mid-stack rule insert emits row signals, never a reset.
    CHECK(resetSpy.count() == 0);
}
