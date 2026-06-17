/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwLineBrushListModel.h"
#include "cwLineBrush.h"
#include "cwSymbologyPalette.h"
#include "cwSymbologyPaletteData.h"

// Qt
#include <QVector>

namespace {

cwLineBrush makeBrush(const QString &name, const QString &displayName,
                      const QString &category, int zOrder)
{
    cwLineBrush brush;
    brush.name = name;
    brush.displayName = displayName;
    brush.category = category;
    brush.zOrder = zOrder;
    return brush;
}

// Three categories with interleaved zOrders so the sort has work to do:
//   Walls    (min zOrder 0): Wall(0), Bedrock(5)
//   Features (min zOrder 3): Feature(10), Anchor(3)
//   Symbols  (min zOrder 20): Water(20)
// Expected flattened order — categories by min zOrder, then displayName
// locale-aware within each:
//   Bedrock, Wall, Anchor, Feature, Water
QVector<cwLineBrush> unsortedBrushes()
{
    return {
        makeBrush(QStringLiteral("water"),   QStringLiteral("Water"),   QStringLiteral("Symbols"),  20),
        makeBrush(QStringLiteral("wall"),    QStringLiteral("Wall"),    QStringLiteral("Walls"),     0),
        makeBrush(QStringLiteral("feature"), QStringLiteral("Feature"), QStringLiteral("Features"), 10),
        makeBrush(QStringLiteral("bedrock"), QStringLiteral("Bedrock"), QStringLiteral("Walls"),     5),
        makeBrush(QStringLiteral("anchor"),  QStringLiteral("Anchor"),  QStringLiteral("Features"),  3)
    };
}

QStringList displayNames(const QVector<cwLineBrush> &brushes)
{
    QStringList names;
    for (const cwLineBrush &brush : brushes) {
        names << brush.displayName;
    }
    return names;
}

} // namespace

TEST_CASE("Brush picker sort orders categories by min zOrder then displayName",
          "[cwLineBrushListModel]")
{
    const QVector<cwLineBrush> sorted = cwLineBrushListModel::sortedForPicker(unsortedBrushes());

    CHECK(displayNames(sorted) == QStringList({
        QStringLiteral("Bedrock"), QStringLiteral("Wall"),       // Walls (min 0)
        QStringLiteral("Anchor"),  QStringLiteral("Feature"),    // Features (min 3)
        QStringLiteral("Water")                                  // Symbols (min 20)
    }));
}

TEST_CASE("LineBrushListModel exposes brushes with category boundaries",
          "[cwLineBrushListModel]")
{
    cwSymbologyPaletteData data;
    data.name = QStringLiteral("Test");
    data.lineBrushes = unsortedBrushes();

    cwSymbologyPalette palette;
    palette.setData(data);

    cwLineBrushListModel model;
    model.setPalette(&palette);

    REQUIRE(model.rowCount() == 5);

    const auto displayAt = [&](int row) {
        return model.data(model.index(row, 0), cwLineBrushListModel::DisplayNameRole).toString();
    };
    const auto firstInCategoryAt = [&](int row) {
        return model.data(model.index(row, 0), cwLineBrushListModel::IsFirstInCategoryRole).toBool();
    };

    CHECK(displayAt(0) == QStringLiteral("Bedrock"));
    CHECK(displayAt(4) == QStringLiteral("Water"));

    // One header per category: rows 0 (Walls), 2 (Features), 4 (Symbols).
    CHECK(firstInCategoryAt(0));
    CHECK_FALSE(firstInCategoryAt(1));
    CHECK(firstInCategoryAt(2));
    CHECK_FALSE(firstInCategoryAt(3));
    CHECK(firstInCategoryAt(4));
}

TEST_CASE("LineBrushListModel empties when the palette is cleared",
          "[cwLineBrushListModel]")
{
    cwSymbologyPaletteData data;
    data.lineBrushes = unsortedBrushes();

    cwSymbologyPalette palette;
    palette.setData(data);

    cwLineBrushListModel model;
    model.setPalette(&palette);
    REQUIRE(model.rowCount() == 5);

    model.setPalette(nullptr);
    CHECK(model.rowCount() == 0);
}
