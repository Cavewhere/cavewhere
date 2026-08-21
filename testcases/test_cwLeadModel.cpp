//Catch includes
#include <catch2/catch_test_macros.hpp>
#include "TestHelper.h"

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwLead.h"
#include "cwLeadModel.h"
#include "cwNote.h"
#include "cwRootData.h"
#include "cwScrap.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"

//Qt includes
#include <QSignalSpy>

namespace {

    constexpr QSizeF kLeadSize(1.0, 1.0);
    const QString kFirstNote = QStringLiteral("First");
    const QString kSecondNote = QStringLiteral("Second");

    QString leadDescription(const QString& name, int index)
    {
        return QStringLiteral("%1 lead %2").arg(name).arg(index);
    }

    QStringList leadDescriptions(const QString& name, int leadCount)
    {
        QStringList descriptions;
        for(int i = 0; i < leadCount; i++) {
            descriptions.append(leadDescription(name, i));
        }
        return descriptions;
    }

    //! A scrap already holding \a leadCount leads, the way loading and undo hand one over
    cwScrap* scrapWithLeads(const QString& name, int leadCount)
    {
        auto* scrap = new cwScrap();
        for(int i = 0; i < leadCount; i++) {
            cwLead lead;
            lead.setPositionOnNote(QPointF(i, i));
            lead.setSize(kLeadSize);
            lead.setDescription(leadDescription(name, i));
            scrap->addLead(lead);
        }
        return scrap;
    }

    cwNote* addNoteWithLeads(const TestHelper& helper,
                             cwTrip* trip,
                             const QString& name,
                             int leadCount)
    {
        cwNote* note = helper.addNoteWithScrap(trip, name);
        REQUIRE(note != nullptr);
        for(int i = 0; i < leadCount; i++) {
            REQUIRE(helper.addScrapLead(note, 0, QPointF(i, i), kLeadSize, leadDescription(name, i)));
        }
        return note;
    }

    QStringList modelLeadDescriptions(const cwLeadModel& model)
    {
        QStringList descriptions;
        for(int row = 0; row < model.rowCount(); row++) {
            descriptions.append(model.data(model.index(row), cwLeadModel::LeadDesciption).toString());
        }
        return descriptions;
    }
}

//Regression test for issue #662: cwLeadModel changes its row count when a
//scrap comes or goes, so it has to say so the way every other model does.
//
//Each row count is checked before the rows themselves: stale offsets send
//scrapAndIndex() off the front of OffsetToScrap, so a wrong count has to stop
//the section rather than fall through to reading rows.
TEST_CASE("cwLeadModel should follow scraps coming and going", "[cwLeadModel]")
{
    TestHelper helper;
    auto rootData = std::make_unique<cwRootData>();

    auto* region = rootData->region();
    region->addCave();
    cwCave* cave = region->cave(0);
    cave->addTrip();
    cwTrip* trip = cave->trip(0);

    cwNote* firstNote = addNoteWithLeads(helper, trip, kFirstNote, 3);

    cwLeadModel model;
    model.setRegionTreeModel(rootData->regionTreeModel());
    model.setCave(cave);

    REQUIRE(model.rowCount() == 3);

    QSignalSpy countSpy(&model, &cwLeadModel::countChanged);

    SECTION("Adding a scrap that already holds leads") {
        firstNote->addScrap(scrapWithLeads(kSecondNote, 2));

        CHECK(countSpy.count() >= 1);
        REQUIRE(model.rowCount() == 5);
        CHECK(modelLeadDescriptions(model)
              == leadDescriptions(kFirstNote, 3) + leadDescriptions(kSecondNote, 2));
    }

    //Both removals take the same 3 leads away, leaving the second note's 2
    const auto checkOnlySecondNoteLeadsRemain = [&]() {
        CHECK(countSpy.count() >= 1);
        REQUIRE(model.rowCount() == 2);
        CHECK(modelLeadDescriptions(model) == leadDescriptions(kSecondNote, 2));
    };

    SECTION("Removing the scrap that holds the leads") {
        addNoteWithLeads(helper, trip, kSecondNote, 2);
        REQUIRE(model.rowCount() == 5);
        countSpy.clear();

        firstNote->removeScraps(0, 0);

        checkOnlySecondNoteLeadsRemain();
    }

    SECTION("Removing the note that holds the leads") {
        addNoteWithLeads(helper, trip, kSecondNote, 2);
        REQUIRE(model.rowCount() == 5);
        countSpy.clear();

        trip->notes()->removeNote(0);

        checkOnlySecondNoteLeadsRemain();
    }
}
