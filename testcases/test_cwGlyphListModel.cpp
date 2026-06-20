/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwGlyphListModel.h"
#include "cwPenStroke.h"
#include "cwSymbologyGlyph.h"
#include "cwSymbologyPalette.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteManager.h"

// Qt
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>

namespace {

// A glyph whose name/displayName/strokeCount are all observable through the
// model's three roles. The strokes are default-constructed — only their count
// matters here.
cwSymbologyGlyph makeGlyph(const QString &name, const QString &displayName, int strokeCount)
{
    cwSymbologyGlyph glyph;
    glyph.name = name;
    glyph.displayName = displayName;
    glyph.strokes = QVector<cwPenStroke>(strokeCount);
    return glyph;
}

// A heap palette pre-loaded with the given glyphs via setData(), so the model's
// initial scan and its object-first refresh path can both be exercised.
cwSymbologyPalette *makePalette(QObject *parent, const QVector<cwSymbologyGlyph> &glyphs)
{
    cwSymbologyPaletteData data;
    data.id = QUuid::createUuid();
    data.name = QStringLiteral("Test Palette");
    data.glyphs = glyphs;

    auto *palette = new cwSymbologyPalette(parent);
    palette->setData(data);
    palette->setWritable(true);
    return palette;
}

// Restores the singleton manager so the manager-driven case never leaks an
// installed directory into sibling tests.
struct SingletonGuard {
    ~SingletonGuard()
    {
        cwSymbologyPaletteManager::instance()->setPaletteDirectory(QString());
    }
};

} // namespace

TEST_CASE("cwGlyphListModel exposes one sorted row per glyph", "[cwGlyphListModel]")
{
    QObject owner;
    auto *palette = makePalette(&owner, {
        makeGlyph(QStringLiteral("ceiling-step"), QStringLiteral("Ceiling Step"), 3),
        makeGlyph(QStringLiteral("anchor"), QStringLiteral("Anchor"), 1),
        makeGlyph(QStringLiteral("blob"), QStringLiteral("blob"), 0)
    });

    cwGlyphListModel model;
    model.setPalette(palette);

    REQUIRE(model.rowCount() == 3);

    // Sorted alphabetically by displayName (locale-aware): Anchor, blob,
    // Ceiling Step.
    auto displayAt = [&](int row) {
        return model.data(model.index(row), cwGlyphListModel::DisplayNameRole).toString();
    };
    CHECK(displayAt(0) == QStringLiteral("Anchor"));
    CHECK(displayAt(1) == QStringLiteral("blob"));
    CHECK(displayAt(2) == QStringLiteral("Ceiling Step"));

    // Each role returns the matching field for the row.
    CHECK(model.data(model.index(0), cwGlyphListModel::NameRole).toString()
          == QStringLiteral("anchor"));
    CHECK(model.data(model.index(0), cwGlyphListModel::StrokeCountRole).toInt() == 1);
    CHECK(model.data(model.index(2), cwGlyphListModel::NameRole).toString()
          == QStringLiteral("ceiling-step"));
    CHECK(model.data(model.index(2), cwGlyphListModel::StrokeCountRole).toInt() == 3);
}

TEST_CASE("cwGlyphListModel indexOfName finds the row in sorted order", "[cwGlyphListModel]")
{
    QObject owner;
    auto *palette = makePalette(&owner, {
        makeGlyph(QStringLiteral("ceiling-step"), QStringLiteral("Ceiling Step"), 3),
        makeGlyph(QStringLiteral("anchor"), QStringLiteral("Anchor"), 1)
    });

    cwGlyphListModel model;
    model.setPalette(palette);

    // Sorted: Anchor (row 0), Ceiling Step (row 1).
    CHECK(model.indexOfName(QStringLiteral("anchor")) == 0);
    CHECK(model.indexOfName(QStringLiteral("ceiling-step")) == 1);
    CHECK(model.indexOfName(QStringLiteral("missing")) == -1);
    CHECK(model.indexOfName(QString()) == -1);

    // It tracks the row across a re-sorting edit, not a fixed position.
    palette->setGlyph(makeGlyph(QStringLiteral("aaa"), QStringLiteral("Aaa"), 0));
    CHECK(model.indexOfName(QStringLiteral("aaa")) == 0);
    CHECK(model.indexOfName(QStringLiteral("anchor")) == 1);
}

TEST_CASE("cwGlyphListModel exposes the three role names", "[cwGlyphListModel]")
{
    cwGlyphListModel model;
    const auto roles = model.roleNames();
    CHECK(roles.value(cwGlyphListModel::NameRole) == QByteArray("name"));
    CHECK(roles.value(cwGlyphListModel::DisplayNameRole) == QByteArray("displayName"));
    CHECK(roles.value(cwGlyphListModel::StrokeCountRole) == QByteArray("strokeCount"));
}

TEST_CASE("cwGlyphListModel sortedForList orders by displayName without a model",
          "[cwGlyphListModel]")
{
    const QVector<cwSymbologyGlyph> sorted = cwGlyphListModel::sortedForList({
        makeGlyph(QStringLiteral("c"), QStringLiteral("Charlie"), 0),
        makeGlyph(QStringLiteral("a"), QStringLiteral("alpha"), 0),
        makeGlyph(QStringLiteral("b"), QStringLiteral("Bravo"), 0)
    });

    REQUIRE(sorted.size() == 3);
    CHECK(sorted.at(0).displayName == QStringLiteral("alpha"));
    CHECK(sorted.at(1).displayName == QStringLiteral("Bravo"));
    CHECK(sorted.at(2).displayName == QStringLiteral("Charlie"));
}

TEST_CASE("cwGlyphListModel refreshes on object-first glyph edits", "[cwGlyphListModel]")
{
    QObject owner;
    auto *palette = makePalette(&owner, {
        makeGlyph(QStringLiteral("anchor"), QStringLiteral("Anchor"), 1)
    });

    cwGlyphListModel model;
    model.setPalette(palette);
    REQUIRE(model.rowCount() == 1);

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    // Upsert a new glyph — object-first setGlyph emits glyphChanged (NOT
    // dataChanged), which a dataChanged-only wiring would miss.
    palette->setGlyph(makeGlyph(QStringLiteral("blob"), QStringLiteral("Blob"), 2));
    CHECK(model.rowCount() == 2);
    CHECK(resetSpy.count() == 1);

    // Editing an existing glyph in place is reflected too.
    palette->setGlyph(makeGlyph(QStringLiteral("anchor"), QStringLiteral("Anchor"), 5));
    CHECK(model.rowCount() == 2);
    auto strokeCountFor = [&](const QString &displayName) {
        for (int row = 0; row < model.rowCount(); ++row) {
            if (model.data(model.index(row), cwGlyphListModel::DisplayNameRole).toString() == displayName) {
                return model.data(model.index(row), cwGlyphListModel::StrokeCountRole).toInt();
            }
        }
        return -1;
    };
    CHECK(strokeCountFor(QStringLiteral("Anchor")) == 5);

    // Removing a glyph drops its row.
    palette->removeGlyph(QStringLiteral("anchor"));
    CHECK(model.rowCount() == 1);
    CHECK(model.data(model.index(0), cwGlyphListModel::NameRole).toString()
          == QStringLiteral("blob"));
}

TEST_CASE("cwGlyphListModel tracks manager saveGlyph/removeGlyph", "[cwGlyphListModel]")
{
    SingletonGuard guard;

    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    auto *manager = cwSymbologyPaletteManager::instance();
    manager->setPaletteDirectory(temp.path());

    // Fork the shipped default into a writable palette (in memory; the async
    // persistence isn't needed for the model's reaction).
    cwSymbologyPalette *palette = manager->duplicatePalette(manager->defaultPalette(),
                                                            QStringLiteral("My Glyphs"));
    REQUIRE(palette != nullptr);
    REQUIRE(palette->isWritable());

    cwGlyphListModel model;
    model.setPalette(palette);
    const int baseline = model.rowCount();

    REQUIRE(manager->saveGlyph(palette, makeGlyph(QStringLiteral("zzz-new"),
                                                  QStringLiteral("Zzz New"), 1)));
    CHECK(model.rowCount() == baseline + 1);
    CHECK(model.data(model.index(model.rowCount() - 1), cwGlyphListModel::NameRole).toString()
          == QStringLiteral("zzz-new"));

    REQUIRE(manager->removeGlyph(palette, QStringLiteral("zzz-new")));
    CHECK(model.rowCount() == baseline);
}
