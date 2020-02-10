//Our includes
#include "cwLead.h"

//Catch includes
#include "catch.hpp"

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
