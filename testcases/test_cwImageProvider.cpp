//Catch2 includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "LoadProjectHelper.h"

//Qt includes
#include <QCoreApplication>
#include <QImage>
#include <QSize>
#include <QString>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QElapsedTimer>
#include <QImageReader>
#include <QTemporaryDir>
#include <QUrl>
#include <cmath>

//Our includes
#include "cwImageProvider.h"
#include "cwImage.h"
#include "cwPDFSettings.h"
#include "cwSvgReader.h"
#include "cwUnits.h"
#include "TestHelper.h"

TEST_CASE("test cwImageProvider requestImage cache behavior", "[cwImageProvider]") {
    // 1) Copy the resource into a temp directory and derive paths
    const QString filename = copyToTempFolder(testcasesDatasetPath("test_cwImageProvider/testpage.png"));
    REQUIRE(!filename.isEmpty());

    const QFileInfo fileInfo(filename);
    REQUIRE(fileInfo.exists());
    REQUIRE(fileInfo.isFile());

    //ProjectFile doesn't exist, but that's not need for this testcase to work
    const QString projectFile = fileInfo.absolutePath() + QStringLiteral("/test.cwproj");
    const QString imageRelativeName = fileInfo.fileName(); // "testpage.png"

    // 2) Set up provider with project path
    cwImageProvider provider;
    // provider.setProjectPath(projectFile);
    provider.setDataRootDir(fileInfo.absoluteDir());

    // 3) Request original image with no requested size (QSize())
    {
        QSize returnedSize;
        QImage image = provider.requestImage(imageRelativeName, &returnedSize, QSize());
        REQUIRE(!image.isNull());
        REQUIRE(returnedSize.isValid());
        REQUIRE(image.size() == returnedSize);
    }

    // 4) Prepare expected cache file path for the resized request
    const QString cacheDirectoryPath = QDir(fileInfo.absolutePath()).filePath(".cw_cache");
    const QString expectedCacheFilename = "scaled-256_300-testpage.png.cw_img_cache";
    const QString expectedCachePath = QDir(cacheDirectoryPath).filePath(expectedCacheFilename);

    // Ensure a clean slate for the size-specific cache so the next load is a miss
    {
        QFile existing(expectedCachePath);
        // qDebug() << "File exists:" << expectedCachePath << existing.exists();
        if (existing.exists()) {
            REQUIRE(existing.remove());
            REQUIRE(!existing.exists());
        }
    }

    // 5) First load at 256x300 (cache miss, should create cache file)
    qint64 missNanoseconds = 0;
    {
        QElapsedTimer timer;
        timer.start();

        QSize returnedSize;
        QImage image = provider.requestImage(imageRelativeName, &returnedSize, QSize(256, 300));

        missNanoseconds = timer.nsecsElapsed();

        REQUIRE(!image.isNull());
        REQUIRE(returnedSize == QSize(300, 235));
        REQUIRE(image.size() == QSize(300, 235));
    }

    // 6) Verify cache file exists and is non-empty
    {
        const QFileInfo cacheInfo(expectedCachePath);
        INFO("Expected Cache location:" << expectedCachePath.toStdString());
        REQUIRE(cacheInfo.exists());
        REQUIRE(cacheInfo.isFile());
        REQUIRE(cacheInfo.size() > 0);
    }

    // 7) Second load at 235x300 (cache hit, should be faster)
    qint64 hitNanoseconds = 0;
    {
        QElapsedTimer timer;
        timer.start();

        QSize returnedSize;
        QImage image = provider.requestImage(imageRelativeName, &returnedSize, QSize(256, 300));

        hitNanoseconds = timer.nsecsElapsed();

        REQUIRE(!image.isNull());
        REQUIRE(returnedSize == QSize(300, 235));
        REQUIRE(image.size() == QSize(300, 235));
    }

    // 8) Performance check: cache hit should be faster than miss
    INFO("Cache miss time (ns): " << missNanoseconds);
    INFO("Cache hit  time (ns): " << hitNanoseconds);
    REQUIRE(hitNanoseconds < missNanoseconds);
}

TEST_CASE("cwImageProvider renders SVG using resolutionImport", "[cwImageProvider]") {
    const QString filename = copyToTempFolder(testcasesDatasetPath("test_cwImageProvider/supportedImage.svg"));
    REQUIRE(!filename.isEmpty());

    const QFileInfo fileInfo(filename);
    REQUIRE(fileInfo.exists());
    REQUIRE(fileInfo.isFile());

    const QString projectFile = fileInfo.absolutePath() + QStringLiteral("/test.cwproj");
    const QString imageRelativeName = fileInfo.fileName();

    cwImageProvider provider;
    provider.setDataRootDir(fileInfo.absoluteDir());

    cwPDFSettings::initialize();
    auto settings = cwPDFSettings::instance();
    const int originalResolution = settings->resolutionImport();
    const int resolutionPpi = 144;
    settings->setResolutionImport(resolutionPpi);

    QImageReader baseReader(filename);
    const QSize baseSize = baseReader.size();
    REQUIRE(baseSize.isValid());

    const double scale = resolutionPpi / 96.0;
    const QSize expectedSize(
        std::max(1, qRound(baseSize.width() * scale)),
        std::max(1, qRound(baseSize.height() * scale)));

    QSize returnedSize;
    QImage image = provider.requestImage(imageRelativeName, &returnedSize, QSize());
    REQUIRE(!image.isNull());
    CHECK(image.size() == expectedSize);
    CHECK(returnedSize == expectedSize);

    const int expectedDotsPerMeter = qRound(cwUnits::convert(
        resolutionPpi,
        cwUnits::DotsPerInch,
        cwUnits::DotsPerMeter));
    CHECK(image.dotsPerMeterX() == expectedDotsPerMeter);
    CHECK(image.dotsPerMeterY() == expectedDotsPerMeter);

    settings->setResolutionImport(originalResolution);
}

TEST_CASE("cwImageProvider respects SVG unit sizes with CSS dpi", "[cwImageProvider]") {
    const QString filename = copyToTempFolder(testcasesDatasetPath("test_cwImageProvider/2019c154_-_party_fault-2p.svg"));
    REQUIRE(!filename.isEmpty());

    const QFileInfo fileInfo(filename);
    REQUIRE(fileInfo.exists());
    REQUIRE(fileInfo.isFile());

    const QString projectFile = fileInfo.absolutePath() + QStringLiteral("/test.cwproj");
    const QString imageRelativeName = fileInfo.fileName();

    cwImageProvider provider;
    provider.setDataRootDir(fileInfo.absoluteDir());

    cwPDFSettings::initialize();
    auto settings = cwPDFSettings::instance();
    const int originalResolution = settings->resolutionImport();
    const int resolutionPpi = 96;
    settings->setResolutionImport(resolutionPpi);

    const double widthInches = 210.0 / 25.4;
    const double heightInches = 297.0 / 25.4;
    const QSize expectedSize(
        std::max(1, qRound(widthInches * resolutionPpi)),
        std::max(1, qRound(heightInches * resolutionPpi)));

    QSize returnedSize;
    QImage image = provider.requestImage(imageRelativeName, &returnedSize, QSize());
    REQUIRE(!image.isNull());
    CHECK(image.size() == expectedSize);
    CHECK(returnedSize == expectedSize);

    settings->setResolutionImport(originalResolution);
}

TEST_CASE("cwImageProvider clamps SVG render size to 256MB threshold", "[cwImageProvider]") {
    const QString filename = copyToTempFolder(testcasesDatasetPath("test_cwImageProvider/2019c154_-_party_fault-2p.svg"));
    REQUIRE(!filename.isEmpty());

    const QFileInfo fileInfo(filename);
    REQUIRE(fileInfo.exists());
    REQUIRE(fileInfo.isFile());

    const QString projectFile = fileInfo.absolutePath() + QStringLiteral("/test.cwproj");
    const QString imageRelativeName = fileInfo.fileName();

    cwImageProvider provider;
    provider.setDataRootDir(fileInfo.absoluteDir());

    cwPDFSettings::initialize();
    auto settings = cwPDFSettings::instance();
    const int originalResolution = settings->resolutionImport();
    const int resolutionPpi = 900;
    settings->setResolutionImport(resolutionPpi);

    const double widthInches = 210.0 / 25.4;
    const double heightInches = 297.0 / 25.4;
    const QSize scaledSize(
        std::max(1, qRound(widthInches * resolutionPpi)),
        std::max(1, qRound(heightInches * resolutionPpi)));

    const qint64 maxPixels = (256ll * 1024 * 1024) / 4;
    const qint64 pixelCount = static_cast<qint64>(scaledSize.width()) * scaledSize.height();
    REQUIRE(pixelCount > maxPixels);

    const double scale = std::sqrt(static_cast<double>(maxPixels) / static_cast<double>(pixelCount));
    const QSize expectedSize(
        std::max(1, static_cast<int>(std::floor(scaledSize.width() * scale))),
        std::max(1, static_cast<int>(std::floor(scaledSize.height() * scale))));

    QSize returnedSize;
    QImage image = provider.requestImage(imageRelativeName, &returnedSize, QSize());
    REQUIRE(!image.isNull());
    CHECK(image.size() == expectedSize);
    CHECK(returnedSize == expectedSize);

    settings->setResolutionImport(originalResolution);
}

TEST_CASE("cwImageProvider scales SVG units correctly", "[cwImageProvider]") {
    struct UnitCase {
        const char* unit;
        double width;
        double height;
        double widthInches;
        double heightInches;
    };

    const UnitCase cases[] = {
        {"px", 192.0, 288.0, 192.0 / 96.0, 288.0 / 96.0},
        {"in", 2.0, 3.0, 2.0, 3.0},
        {"cm", 5.08, 7.62, 2.0, 3.0},
        {"mm", 50.8, 76.2, 2.0, 3.0},
        {"pt", 144.0, 216.0, 2.0, 3.0},
        {"pc", 12.0, 18.0, 2.0, 3.0}
    };

    cwPDFSettings::initialize();
    auto settings = cwPDFSettings::instance();
    const int originalResolution = settings->resolutionImport();
    const int resolutionPpi = 144;
    settings->setResolutionImport(resolutionPpi);

    for (const UnitCase& unitCase : cases) {
        const QString svgContents = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<svg xmlns=\"http://www.w3.org/2000/svg\" "
            "width=\"%1%2\" height=\"%3%4\" viewBox=\"0 0 200 300\">\n"
            "<rect width=\"200\" height=\"300\" fill=\"#000\"/>\n"
            "</svg>\n")
                                        .arg(unitCase.width, 0, 'f', 6)
                                        .arg(QLatin1String(unitCase.unit))
                                        .arg(unitCase.height, 0, 'f', 6)
                                        .arg(QLatin1String(unitCase.unit));

        const QString tempDirPath = QDir::temp().filePath(
            QStringLiteral("cwImageProviderSvgUnits-%1").arg(QCoreApplication::applicationPid()));
        QDir().mkpath(tempDirPath);
        const QString tempPath = QDir(tempDirPath).filePath(
            QStringLiteral("units-%1.svg").arg(QLatin1String(unitCase.unit)));

        QFile svgFile(tempPath);
        REQUIRE(svgFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        REQUIRE(svgFile.write(svgContents.toUtf8()) > 0);
        svgFile.close();

        const QString projectFile = QDir(tempDirPath).filePath(QStringLiteral("test.cwproj"));
        const QString imageRelativeName = QFileInfo(tempPath).fileName();

        cwImageProvider provider;
        provider.setDataRootDir(tempDirPath); //fileInfo.absoluteDir());

        const QSize expectedSize(
            std::max(1, qRound(unitCase.widthInches * resolutionPpi)),
            std::max(1, qRound(unitCase.heightInches * resolutionPpi)));

        QSize returnedSize;
        QImage image = provider.requestImage(imageRelativeName, &returnedSize, QSize());
        REQUIRE(!image.isNull());
        CHECK(image.size() == expectedSize);
        CHECK(returnedSize == expectedSize);

        const int expectedDotsPerMeter = qRound(cwUnits::convert(
            resolutionPpi,
            cwUnits::DotsPerInch,
            cwUnits::DotsPerMeter));
        CHECK(image.dotsPerMeterX() == expectedDotsPerMeter);
        CHECK(image.dotsPerMeterY() == expectedDotsPerMeter);
    }

    settings->setResolutionImport(originalResolution);
}

namespace {

//MediaBox of the second page of test_cwPDFConverter/2page-test.pdf; the first page is square
constexpr QSizeF secondPagePoints(150.0, 100.0);
constexpr int secondPageIndex = 1;

} // namespace

TEST_CASE("cwImageProvider loads images from folders named with #, % and spaces", "[cwImageProvider]") {
    const QString folder = GENERATE(QStringLiteral("Proj#1/Cave#2/trips/Trip#3/notes"),
                                    QStringLiteral("100% cave/notes"),
                                    QStringLiteral("with space/notes"),
                                    //Folders the user can pick for the project root; sanitizeFileName
                                    //only covers project, cave and trip names
                                    QStringLiteral("what?/notes"),
                                    QStringLiteral("what?#/notes"),
                                    QStringLiteral("[bracket]/notes"),
                                    QStringLiteral("a+b/notes"),
                                    QStringLiteral("a&b=c;d/notes"),
                                    QStringLiteral("a%23b/notes"),
                                    QStringLiteral("a%20b/notes"),
                                    QStringLiteral("H\u00f6hle/notes"));
    INFO("Folder:" << folder.toStdString());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir rootDir(tempDir.path());
    REQUIRE(rootDir.mkpath(folder));

    const QString sourcePath = testcasesDatasetSourcePath(QStringLiteral("test_cwImageProvider/testpage.png"));
    const QString absolutePath = QDir(rootDir.filePath(folder)).filePath(QStringLiteral("testpage.png"));
    REQUIRE(QFile::copy(sourcePath, absolutePath));

    const QSize expectedSize = QImageReader(sourcePath).size();
    REQUIRE(expectedSize.isValid());

    cwImageProvider provider;
    provider.setDataRootDir(rootDir);

    const QString imageUrl = cwImageProvider::imageUrl(cwImage(), absolutePath);
    const QString id = imageProviderRequestId(imageUrl);
    INFO("Url:" << imageUrl.toStdString());
    INFO("Id:" << id.toStdString());

    //A png carries no #page fragment, so the whole id is the encoded path. A raw ? or #
    //would be split off by parseImageRequest and cut the path short.
    CHECK(!id.contains(QLatin1Char('?')));
    CHECK(!id.contains(QLatin1Char('#')));
    CHECK(QUrl::fromPercentEncoding(id.toUtf8()).toStdString() == absolutePath.toStdString());

    QSize returnedSize;
    const QImage image = provider.requestImage(id, &returnedSize, QSize());
    REQUIRE(!image.isNull());
    CHECK(image.size() == expectedSize);
    CHECK(returnedSize == expectedSize);
}

TEST_CASE("cwImageProvider request id keeps the PDF page as the final fragment", "[cwImageProvider]") {
    const QString absolutePath = QStringLiteral("/tmp/Proj#1/100% cave/trips/Trip #3/notes/scan.pdf");

    cwImage image;
    image.setPage(secondPageIndex);

    const QString id = cwImageProvider::imageRequestId(image, absolutePath);
    INFO("Id:" << id.toStdString());

    CHECK(id.endsWith(QStringLiteral("#page=%1").arg(secondPageIndex + 1)));

    const int fragmentIndex = id.lastIndexOf(QLatin1Char('#'));
    REQUIRE(fragmentIndex >= 0);
    CHECK(id.indexOf(QLatin1Char('#')) == fragmentIndex);
    CHECK(QUrl::fromPercentEncoding(id.left(fragmentIndex).toUtf8()).toStdString() == absolutePath.toStdString());
}

#ifdef CW_WITH_PDF_SUPPORT
TEST_CASE("cwImageProvider renders a PDF page from a folder named with #", "[cwImageProvider]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir rootDir(tempDir.path());
    const QString folder = QStringLiteral("Proj#1/Cave#2/trips/Trip#3/notes");
    REQUIRE(rootDir.mkpath(folder));

    const QString absolutePath = QDir(rootDir.filePath(folder)).filePath(QStringLiteral("2page-test.pdf"));
    REQUIRE(QFile::copy(testcasesDatasetSourcePath(QStringLiteral("test_cwPDFConverter/2page-test.pdf")), absolutePath));

    cwPDFSettings::initialize();
    const int resolutionPpi = cwPDFSettings::instance()->resolutionImport();
    const double scale = resolutionPpi / cwUnits::PointsPerInch;
    const QSize expectedSize(qRound(secondPagePoints.width() * scale),
                             qRound(secondPagePoints.height() * scale));

    cwImageProvider provider;
    provider.setDataRootDir(rootDir);

    cwImage image;
    image.setPage(secondPageIndex);
    const QString id = imageProviderRequestId(cwImageProvider::imageUrl(image, absolutePath));
    INFO("Id:" << id.toStdString());

    QSize returnedSize;
    const QImage rendered = provider.requestImage(id, &returnedSize, QSize());
    REQUIRE(!rendered.isNull());
    CHECK(rendered.size() == expectedSize);
    CHECK(returnedSize == expectedSize);
}
#endif

TEST_CASE("cwSvgReader identifies SVG formats", "[cwSvgReader]") {
    CHECK(cwSvgReader::isSvg(QByteArrayLiteral("svg")) == true);
    CHECK(cwSvgReader::isSvg(QByteArrayLiteral("SVG")) == true);
    CHECK(cwSvgReader::isSvg(QByteArrayLiteral("SvG")) == true);
    CHECK(cwSvgReader::isSvg(QByteArrayLiteral("png")) == false);
    CHECK(cwSvgReader::isSvg(QByteArray()) == false);
}

TEST_CASE("cwSvgReader toImage uses resolutionImport", "[cwSvgReader]") {
    const QString svgContents = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "width=\"2in\" height=\"3in\" viewBox=\"0 0 200 300\">\n"
        "<rect width=\"200\" height=\"300\" fill=\"#000\"/>\n"
        "</svg>\n");

    QByteArray data = svgContents.toUtf8();

    cwPDFSettings::initialize();
    auto settings = cwPDFSettings::instance();
    const int originalResolution = settings->resolutionImport();
    const int resolutionPpi = 144;
    settings->setResolutionImport(resolutionPpi);

    QImage image = cwSvgReader::toImage(data, QByteArrayLiteral("svg"));
    REQUIRE(!image.isNull());
    CHECK(image.size() == QSize(288, 432));

    const int expectedDotsPerMeter = qRound(cwUnits::convert(
        resolutionPpi,
        cwUnits::DotsPerInch,
        cwUnits::DotsPerMeter));
    CHECK(image.dotsPerMeterX() == expectedDotsPerMeter);
    CHECK(image.dotsPerMeterY() == expectedDotsPerMeter);

    settings->setResolutionImport(originalResolution);
}

TEST_CASE("cwSvgReader imageInfo uses native SVG units", "[cwSvgReader]") {
    const QString svgContents = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "width=\"2in\" height=\"3in\" viewBox=\"0 0 200 300\">\n"
        "<rect width=\"200\" height=\"300\" fill=\"#000\"/>\n"
        "</svg>\n");

    QByteArray data = svgContents.toUtf8();
    const cwImage::OriginalImageInfo info = cwSvgReader::imageInfo(data, QByteArrayLiteral("svg"));

    CHECK(info.unit == cwImage::Unit::SvgUnits);
    CHECK(info.originalSize == QSize(192, 288));

    const int expectedDotsPerMeter = qRound(cwUnits::convert(
        96.0,
        cwUnits::DotsPerInch,
        cwUnits::DotsPerMeter));
    CHECK(info.originalDotsPerMeter == expectedDotsPerMeter);
}
