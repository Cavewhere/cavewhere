//Catch includes
#include "catch.hpp"

//Our includes
#include "cwPDFConverter.h"
#include "cwAsyncFuture.h"

TEST_CASE("cwPDFConverter should initilize correctly", "[cwPDFConverter]") {
    cwPDFConverter converter;
    CHECK(converter.source().isEmpty());
    CHECK(converter.resolution() == 300);
    CHECK(converter.error().isEmpty());

    auto future = converter.convert();
    CHECK(future.isFinished() == true);
    CHECK(future.isCanceled() == true);

    CHECK(converter.error().toStdString() == "PDF Renderer error: File not found");
}

TEST_CASE("cwPDFConverter should convert PDFs correctly", "[cwPDFConverter]") {
    cwPDFConverter converter;
    converter.setSource("://datasets/test_cwPDFConverter/2page-test.pdf");
    auto future = converter.convert();

    CHECK(converter.error().toStdString() == "");

    REQUIRE(cwAsyncFuture::waitForFinished(future, 5000));

    REQUIRE(future.resultCount() == 2);

    CHECK(future.resultAt(0).size() == QSize(417, 417));
    CHECK(future.resultAt(1).size() == QSize(625, 417));

    QImage page1("://datasets/test_cwPDFConverter/page1-300.png");
    QImage page2("://datasets/test_cwPDFConverter/page2-300.png");

    CHECK(future.resultAt(0) == page1);
    CHECK(future.resultAt(1) == page2);

    SECTION("Set resolution to 100") {
        converter.setResolution(100);

        auto future = converter.convert();

        CHECK(converter.error().toStdString() == "");

        REQUIRE(cwAsyncFuture::waitForFinished(future, 5000));

        REQUIRE(future.resultCount() == 2);

        CHECK(future.resultAt(0).size() == QSize(139, 139));
        CHECK(future.resultAt(1).size() == QSize(208, 139));
    }
}
