// Catch includes
#include <catch2/catch_test_macros.hpp>

// Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"

// Qt includes
#include <QUndoStack>

TEST_CASE("cwCave rename undo cannot produce two caves sharing a name", "[cwCave]")
{
    QUndoStack undoStack;
    cwCavingRegion region;
    region.setUndoStack(&undoStack);

    region.addCave();
    region.addCave();

    cwCave* alpha = region.cave(0);
    cwCave* beta = region.cave(1);
    alpha->setName(QStringLiteral("Alpha"));
    beta->setName(QStringLiteral("Beta"));

    REQUIRE(alpha->name() == QStringLiteral("Alpha"));
    REQUIRE(beta->name() == QStringLiteral("Beta"));

    //Only the two renames below may be undone - walking further would
    //undo the addCave commands and leave these pointers dangling.
    const int baseline = undoStack.index();

    //Free up "Alpha" and hand it to the other cave, so the two renames
    //are individually legal but together depend on ordering.
    alpha->setName(QStringLiteral("Gamma"));
    beta->setName(QStringLiteral("Alpha"));

    REQUIRE(alpha->name() == QStringLiteral("Gamma"));
    REQUIRE(beta->name() == QStringLiteral("Alpha"));

    //Walk the whole stack back and forth. At no point may two caves end
    //up with the same name - that is what evicts a sibling from cwPage's
    //name-keyed child lookup.
    const auto namesAreUnique = [&]() {
        return alpha->name() != beta->name();
    };

    while (undoStack.index() > baseline) {
        undoStack.undo();
        INFO("after undo: " << alpha->name().toStdString()
             << " / " << beta->name().toStdString());
        REQUIRE(namesAreUnique());
    }

    while (undoStack.canRedo()) {
        undoStack.redo();
        INFO("after redo: " << alpha->name().toStdString()
             << " / " << beta->name().toStdString());
        REQUIRE(namesAreUnique());
    }
}
