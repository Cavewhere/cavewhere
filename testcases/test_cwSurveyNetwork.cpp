//Catch includes
#include "catch.hpp"

//Our includes
#include "cwSurveyNetwork.h"

//Qt include
#include <QStringList>
#include <QSet>

TEST_CASE("cwSurveyNetwork should compare changedStation correctly", "[cwSurveyNetwork]") {
    cwSurveyNetwork n1;
    n1.addShot("a1", "a2");
    n1.addShot("a1", "a3");
    n1.addShot("a2", "a4");
    n1.addShot("a4", "a3");

    cwSurveyNetwork n2 = n1;

    CHECK(cwSurveyNetwork::changedStations(n1, n2).isEmpty() == true);

    //Need to detach to do a direct lookup
    n2.clear();
    n2.addShot("a1", "a2");
    n2.addShot("a1", "a3");
    n2.addShot("a2", "a4");
    n2.addShot("a4", "a3");

    CHECK(cwSurveyNetwork::changedStations(n1, n2).isEmpty() == true);

    //Add a new shot
    n2.addShot("a1", "a5");

    CHECK(cwSurveyNetwork::changedStations(n1, n2) == QStringList({"A5", "A1"}));
    CHECK(cwSurveyNetwork::changedStations(n2, n1) == QStringList({"A5", "A1"}));

    n2.addShot("a2", "a3");

    QStringList testList({"A5", "A1", "A2", "A3"});
    QSet<QString> testSet(testList.begin(), testList.end());

    {
        auto changedStations = cwSurveyNetwork::changedStations(n1, n2);
        QSet<QString> networkSet(changedStations.begin(), changedStations.end());

        CHECK(networkSet == testSet);
    }

    {
        auto changedStations = cwSurveyNetwork::changedStations(n2, n1);
        QSet<QString> networkSet(changedStations.begin(), changedStations.end());

        CHECK(networkSet == testSet);
    }
}
