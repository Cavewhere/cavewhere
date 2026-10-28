/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our includes
#include "cwCavernRunner.h"
#include "cwLinePlotErrorCodes.h"

// Qt
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

namespace {

// A PROJ pipeline is an operation, so PROJ builds no transform from it.
const QString kPipelineCs =
    QStringLiteral(R"(*cs custom "+proj=pipeline +step +proj=unitconvert +xy_in=m +xy_out=m")");

const QString kTransformFailure = QStringLiteral("Failed to convert coordinates");

QString writeDriver(const QTemporaryDir& dir, const QString& name, const QStringList& lines)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    file.write(lines.join(QLatin1Char('\n')).toUtf8());
    file.write("\n");
    return path;
}

QStringList fixWithPipelineCsDriver()
{
    return {
        QStringLiteral("*cs out EPSG:32616"),
        kPipelineCs,
        QStringLiteral("*fix a 500000 4000000 100"),
        QStringLiteral("*data normal from to tape compass clino"),
        QStringLiteral("a b 10 0 0"),
    };
}

} // namespace

TEST_CASE("cwCavernRunner reports a *fix whose coordinate system PROJ can't transform",
          "[cwCavernRunner]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString svxPath = writeDriver(dir, QStringLiteral("fixpipeline.svx"),
                                        fixWithPipelineCsDriver());
    REQUIRE_FALSE(svxPath.isEmpty());

    const auto result = cwCavernRunner::run(svxPath, dir.filePath(QStringLiteral("fixpipeline.3d")));

    REQUIRE(result.hasError());
    CHECK(result.errorCode() == static_cast<int>(LinePlotErrorCode::CavernFailed));
    CHECK(result.errorMessage().contains(kTransformFailure));
}

TEST_CASE("cwCavernRunner reports *declination auto in a coordinate system PROJ can't transform",
          "[cwCavernRunner]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString svxPath = writeDriver(dir, QStringLiteral("declpipeline.svx"), {
        QStringLiteral("*cs out EPSG:32616"),
        kPipelineCs,
        QStringLiteral("*declination auto 500000 4000000 100"),
        QStringLiteral("*date 2000.01.01"),
        QStringLiteral("*data normal from to tape compass clino"),
        QStringLiteral("a b 10 0 0"),
    });
    REQUIRE_FALSE(svxPath.isEmpty());

    const auto result = cwCavernRunner::run(svxPath, dir.filePath(QStringLiteral("declpipeline.3d")));

    REQUIRE(result.hasError());
    CHECK(result.errorCode() == static_cast<int>(LinePlotErrorCode::CavernFailed));
    CHECK(result.errorMessage().contains(kTransformFailure));
}

TEST_CASE("cwCavernRunner solves a valid fix after a run whose transform failed",
          "[cwCavernRunner]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString badPath = writeDriver(dir, QStringLiteral("bad.svx"), fixWithPipelineCsDriver());
    const QString goodPath = writeDriver(dir, QStringLiteral("good.svx"), {
        QStringLiteral("*cs out EPSG:32616"),
        QStringLiteral("*cs EPSG:32616"),
        QStringLiteral("*fix a 500000 4000000 100"),
        QStringLiteral("*data normal from to tape compass clino"),
        QStringLiteral("a b 10 0 0"),
    });
    REQUIRE_FALSE(badPath.isEmpty());
    REQUIRE_FALSE(goodPath.isEmpty());

    const auto badResult = cwCavernRunner::run(badPath, dir.filePath(QStringLiteral("bad.3d")));
    REQUIRE(badResult.hasError());
    CHECK(badResult.errorMessage().contains(kTransformFailure));

    const auto goodResult = cwCavernRunner::run(goodPath, dir.filePath(QStringLiteral("good.3d")));
    INFO(goodResult.errorMessage().toStdString());
    REQUIRE_FALSE(goodResult.hasError());
    CHECK(QFileInfo::exists(goodResult.value().output3dPath));
}
