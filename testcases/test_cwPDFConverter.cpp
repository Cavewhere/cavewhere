//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwPDFConverter.h"
#include "asyncfuture.h"

//Std includes
#include <limits>

//Qt includes
#include <QImage>

namespace {
double meanChannelDelta(const QImage &left, const QImage &right)
{
    if(left.size() != right.size()) {
        return std::numeric_limits<double>::infinity();
    }

    QImage leftImage = left.convertToFormat(QImage::Format_ARGB32);
    QImage rightImage = right.convertToFormat(QImage::Format_ARGB32);

    const int width = leftImage.width();
    const int height = leftImage.height();
    const double pixelCount = static_cast<double>(width) * height;
    if(pixelCount == 0.0) {
        return 0.0;
    }

    quint64 totalDelta = 0;
    for(int y = 0; y < height; ++y) {
        const QRgb* leftRow = reinterpret_cast<const QRgb*>(leftImage.constScanLine(y));
        const QRgb* rightRow = reinterpret_cast<const QRgb*>(rightImage.constScanLine(y));
        for(int x = 0; x < width; ++x) {
            const QRgb leftPixel = leftRow[x];
            const QRgb rightPixel = rightRow[x];
            totalDelta += static_cast<quint64>(qAbs(qRed(leftPixel) - qRed(rightPixel)));
            totalDelta += static_cast<quint64>(qAbs(qGreen(leftPixel) - qGreen(rightPixel)));
            totalDelta += static_cast<quint64>(qAbs(qBlue(leftPixel) - qBlue(rightPixel)));
            totalDelta += static_cast<quint64>(qAbs(qAlpha(leftPixel) - qAlpha(rightPixel)));
        }
    }

    return static_cast<double>(totalDelta) / (pixelCount * 4.0);
}
}

TEST_CASE("cwPDFConverter should initilize correctly", "[cwPDFConverter]") {
    cwPDFConverter converter;
    CHECK(converter.source().isEmpty());
    CHECK(converter.resolution() == 300);
    CHECK(converter.error().isEmpty());

    auto future = converter.convert();
    CHECK(future.isFinished() == true);
    CHECK(future.isCanceled() == true);

    if(converter.isSupported()) {
        CHECK(converter.error().toStdString() == "PDF Renderer error: File not found");
    }
}

TEST_CASE("cwPDFConverter should convert PDFs correctly", "[cwPDFConverter]") {
    cwPDFConverter converter;
    converter.setSource("://datasets/test_cwPDFConverter/2page-test.pdf");
    auto future = converter.convert();

    if(converter.isSupported()) {
        CHECK(converter.error().toStdString() == "");

        REQUIRE(AsyncFuture::waitForFinished(future, 5000));

        REQUIRE(future.resultCount() == 2);

        CHECK(future.resultAt(0).size() == QSize(417, 417));
        CHECK(future.resultAt(1).size() == QSize(625, 417));

        QImage page1("://datasets/test_cwPDFConverter/page1-300.png");
        QImage page2("://datasets/test_cwPDFConverter/page2-300.png");

        const double page1Delta = meanChannelDelta(future.resultAt(0), page1);
        const double page2Delta = meanChannelDelta(future.resultAt(1), page2);
        CAPTURE(page1Delta);
        CAPTURE(page2Delta);
        CHECK(page1Delta <= 2.0);
        CHECK(page2Delta <= 2.0);

        SECTION("Set resolution to 100") {
            converter.setResolution(100);

            auto future = converter.convert();

            CHECK(converter.error().toStdString() == "");

            REQUIRE(AsyncFuture::waitForFinished(future, 5000));

            REQUIRE(future.resultCount() == 2);

            CHECK(future.resultAt(0).size() == QSize(139, 139));
            CHECK(future.resultAt(1).size() == QSize(208, 139));
        }
    } else {
        CHECK(future.isFinished() == true);
        CHECK(future.isCanceled() == true);
    }
}
