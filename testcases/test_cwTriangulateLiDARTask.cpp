//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwJobSettings.h"
#include "cwStationPositionLookup.h"
#include "cwTriangulateLiDARInData.h"
#include "cwTriangulateLiDARTask.h"
#include "asyncfuture.h"

//Qt includes
#include <QVector3D>

TEST_CASE("cwTriangulateLiDARTask returns one future per input, in input order", "[cwTriangulateLiDARTask]")
{
    cwJobSettings::initialize();

    //The middle input has no station lookup, so it is the one that errors out
    const int errorIndex = 1;

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("a1"), QVector3D(0.0f, 0.0f, 0.0f));

    QList<cwTriangulateLiDARInData> inputs;
    for(int i = 0; i < 3; i++) {
        cwTriangulateLiDARInData data;
        if(i != errorIndex) {
            data.setStationLookup(lookup);
        }
        data.setGltfFilename(QStringLiteral("missing-scan-%1.gltf").arg(i));
        inputs.append(data);
    }

    const auto futures = cwTriangulateLiDARTask::triangulate(inputs);
    REQUIRE(futures.size() == inputs.size());

    for(int i = 0; i < futures.size(); i++) {
        auto future = futures.at(i);
        AsyncFuture::waitForFinished(future); //Test-only
        REQUIRE(future.resultCount() == 1);

        const auto result = future.result();
        if(i == errorIndex) {
            CHECK(result.hasError());
        } else {
            CHECK_FALSE(result.hasError());
        }
    }
}
