//Catch includes
#include "catch.hpp"

//Our includes
#include "cwCompassBufferGenerator.h"

//Qt includes
#include <QVector3D>
#include <Qt3DRender/QBufferDataGenerator>

class SomeOtherFunctor : public Qt3DRender::QBufferDataGenerator {
public:
    QByteArray operator()() override { return  QByteArray(); }
    bool operator==(const Qt3DRender::QBufferDataGenerator& other) const override {
        return true; //Always return true
    }

    QT3D_FUNCTOR(SomeOtherFunctor)
};

TEST_CASE("cwCompassBufferGenerator should generate data correctly", "[cwCompassBufferGenerator]") {
    cwCompassBufferGenerator generator;

    QByteArray data = generator();
    CHECK(data.isEmpty() == false);

    //Check using a size
    int pointSize = sizeof (QVector3D) * 2;
    CHECK(data.size() % pointSize == 0);

    int totalSize = pointSize * 4 * 2 * 6 * 2; //i=4, j=2, defaultPointSize=6, 2 sides
    CHECK(data.size() == totalSize );
}

TEST_CASE("cwCompassBufferGenerator equals operator should work correctly", "[cwCompassBufferGenerator]") {
    cwCompassBufferGenerator generator1;
    cwCompassBufferGenerator generator2;
    SomeOtherFunctor otherFunctor;
    CHECK((generator1 == otherFunctor) == false);
    CHECK((generator2 == otherFunctor) == false);
    CHECK(generator1() == generator2()); //Check that the data is the same
}
