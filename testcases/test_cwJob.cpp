//Catch includes
#include "catch.hpp"

//Our includes
#include "cwJob.h"

//Qt includes
#include <QCoreApplication>
#include <QObject>

TEST_CASE("cwJob should have the correct context", "[cwJob]") {
    cwJob job;
    CHECK(job.context() == dynamic_cast<QObject*>(QCoreApplication::instance()));

    auto obj1 = new QObject();
    job.setContext(obj1);

    CHECK(job.context() == obj1);

    delete obj1;
    CHECK(job.context() == nullptr);

    auto obj2 = std::make_unique<QObject>();
    cwJob job2(obj2.get());

    CHECK(job2.context() == obj2.get());

}
