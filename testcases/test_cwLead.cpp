//Our includes
#include "cwLead.h"
#include "cwLeadModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwNote.h"
#include "cwRegionTreeModel.h"
#include "cwScrap.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//QVector
#include <QVector>

TEST_CASE("cwLead should initilize correctly", "[cwLead]") {
    QVector<cwLead> leads(1000);

    for(auto lead : leads)  {
        CHECK(lead.size().isValid() == false);
        CHECK(lead.positionOnNote() == QPointF());
        CHECK(lead.desciption() == QString());
        CHECK(lead.completed() == false);
    }
}

// A Section's trips belong to its cave, so the cave's lead list carries the
// leads drawn on their notes, whether the scrap was there when the cave was
// picked or arrived afterward.
TEST_CASE("cwLeadModel lists the leads of a depth-2 trip", "[cwLead]") {
    cwCavingRegion region;
    cwRegionTreeModel treeModel;
    treeModel.setCavingRegion(&region);

    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Alpha"));
    region.addCave(cave);

    cwCave* section = new cwCave();
    section->setName(QStringLiteral("Upper"));
    cave->addNode(section);

    cwTrip* trip = new cwTrip();
    section->addTrip(trip);
    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    chunk->appendShot(cwStation(QStringLiteral("a1")), cwStation(QStringLiteral("a2")), cwShot());

    cwNote* note = new cwNote();
    trip->notes()->addNotes({note});

    auto addScrapWithLead = [note]() {
        cwScrap* scrap = new cwScrap();
        scrap->addLead(cwLead());
        note->addScrap(scrap);
    };
    addScrapWithLead();

    cwLeadModel leadModel;
    leadModel.setRegionTreeModel(&treeModel);
    leadModel.setCave(cave);

    CHECK(leadModel.rowCount() == 1);
    CHECK(leadModel.referanceStation() == QStringLiteral("a1"));

    addScrapWithLead();
    CHECK(leadModel.rowCount() == 2);
}
