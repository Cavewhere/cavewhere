//Catch includes
#include <catch2/catch_test_macros.hpp>

#include <QSignalSpy>
#include <QMatrix4x4>
#include <QMetaObject>
#include <QVector3D>
#include <QFuture>
#include <QSvgGenerator>
#include <QPainter>
#include <QTemporaryFile>
#include <QQuaternion>

//Our includes
#include "cwSurvey2DGeometryRule.h"
#include "cwSurveyNetworkArtifact.h"
#include "cwMatrix4x4Artifact.h"
#include "cwSurvey2DGeometryArtifact.h"
#include "Monad/Result.h"
#include "cwSurveyNetwork.h"
#include "cwAsyncFuture.h"

//SVG export test
#include "LoadProjectHelper.h"
#include "cwSurveyNetworkBuilderRule.h"
#include "cwSurveyDataArtifact.h"
#include "cwAsyncFuture.h"


TEST_CASE("cwSurvey2DGeometryRule: simple network yields non-empty 2D geometry", "[Survey2DGeometryRule]")
{
    // 1) Instantiate rule + artifacts
    cwSurvey2DGeometryRule rule;
    auto surveyNetworkArtifact = new cwSurveyNetworkArtifact(&rule);
    auto matrixArtifact        = new cwMatrix4x4Artifact(&rule);

    // 2) Build a minimal network: one shot A→B, with positions
    cwSurveyNetwork surveyNetwork;
    surveyNetwork.addShot("A", "B");
    surveyNetwork.setPosition("A", QVector3D(0.0f, 0.5f, 0.0f));
    surveyNetwork.setPosition("B", QVector3D(1.0f, 2.0f, 0.0f));

    surveyNetworkArtifact->setSurveyNetwork(QtFuture::makeReadyValueFuture(Monad::Result(surveyNetwork)));

    // 3) Use identity view matrix
    matrixArtifact->setMatrix4x4(QMatrix4x4());

    // 5) Spy on the geometry‐changed signal
    auto survey2DGeometryArtifact = rule.survey2DGeometry();
    QSignalSpy geometryChangedSpy(
        survey2DGeometryArtifact,
        &cwSurvey2DGeometryArtifact::geometryResultChanged
        );

    // 4) Wire them into the rule
    rule.setSurveyNetwork(surveyNetworkArtifact);
    rule.setViewMatrix(matrixArtifact);

    // 7) Expect exactly one notification
    REQUIRE(geometryChangedSpy.count() == 1);

    // 8) Retrieve and verify the computed geometry
    auto geometryFuture = survey2DGeometryArtifact->geometryResult();
    REQUIRE(cwAsyncFuture::waitForFinished(geometryFuture, 2000));

    auto geometryResult = geometryFuture.result();
    REQUIRE(!geometryResult.hasError());

    auto geometryValue = geometryResult.value();
    CHECK(!geometryValue.shotLines.isEmpty());
    CHECK(!geometryValue.stations.isEmpty());

    // 8) There should be exactly one shot line
    REQUIRE(geometryValue.shotLines.size() == 1);
    auto line = geometryValue.shotLines.first();
    bool forward  = (line.p1() == QPointF(0.0, 0.5) && line.p2() == QPointF(1.0, 2.0));
    bool backward = (line.p1() == QPointF(1.0, 2.0) && line.p2() == QPointF(0.0, 0.5));
    REQUIRE((forward || backward) == true);

    // 9) Stations vector must contain exactly two entries: A @ (0,0) and B @ (1,0)
    REQUIRE(geometryValue.stations.size() == 2);

    auto itA = std::find_if(
        geometryValue.stations.begin(),
        geometryValue.stations.end(),
        [](auto const& s){ return s.name == "a"; }
        );
    REQUIRE(itA != geometryValue.stations.end());
    CHECK(itA->position == QPointF(0.0, 0.5));

    auto itB = std::find_if(
        geometryValue.stations.begin(),
        geometryValue.stations.end(),
        [](auto const& s){ return s.name == "b"; }
        );
    REQUIRE(itB != geometryValue.stations.end());
    CHECK(itB->position == QPointF(1.0, 2.0));
}

TEST_CASE("SVG export test", "[Survey2DGeometryRule]") {
    auto project = fileToProject("://datasets/test_cwProject/Phake Cave 3000.cw");
    auto cavingRegion = project->cavingRegion();

    // Create a survey data artifact and set the region
    cwSurveyDataArtifact surveyData;
    surveyData.setRegion(cavingRegion);

    cwSurveyNetworkBuilderRule networkBuilderRule;
    networkBuilderRule.setSurveyData(&surveyData);

    cwSurvey2DGeometryRule rule;
    auto matrixArtifact = new cwMatrix4x4Artifact(&rule);

    double pitch = 0.0;
    double azimuth = 90.0;

    auto rotation = [](double pitch, double azimuth) {
        QQuaternion pitchQuat = QQuaternion::fromAxisAndAngle(1.0, 0.0, 0.0, pitch);
        QQuaternion azimuthQuat = QQuaternion::fromAxisAndAngle(0.0, 0.0, 1.0, azimuth);
        return pitchQuat * azimuthQuat;
    };

    auto defaultQuat = rotation(90.0, 0.0);
    auto rotationQuat = rotation(pitch, azimuth);
    QQuaternion rotationDifferance = defaultQuat.conjugated() * rotationQuat;

    QMatrix4x4 matrix;
    matrix.rotate(rotationDifferance);
    matrixArtifact->setMatrix4x4(matrix);

    rule.setSurveyNetwork(networkBuilderRule.surveyNetworkArtifact());
    rule.setViewMatrix(matrixArtifact);

    auto geometryFuture = rule.survey2DGeometry()->geometryResult();
    REQUIRE(cwAsyncFuture::waitForFinished(geometryFuture, 2000));

    REQUIRE(!geometryFuture.result().hasError());
    const cwSurvey2DGeometry geometry = geometryFuture.result().value();

    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = -std::numeric_limits<double>::max();
    double maxY = -std::numeric_limits<double>::max();


    for (auto const& station : geometry.stations) {
        double x = station.position.x();
        double y = station.position.y();

        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }

    double width = maxX - minX;
    double height = maxY - minY;

    // 4) Set up an SVG generator that writes to ~/Desktop/test.svg
    auto svgPath = QDir::tempPath() + "/Survey2DGeometryRule-test.svg";
    QSvgGenerator generator;
    generator.setFileName(svgPath);
    generator.setSize(QSize(width, height));

    auto flip = [height, minY](const QPointF point) {
        return QPointF(point.x(), height - point.y());
    };

    QRectF viewBox(QPointF(0.0, 0.0), QPointF(maxX, maxY));
    generator.setViewBox(viewBox);
    generator.setTitle("Cave Survey 2D Geometry");
    generator.setDescription("Auto‑generated SVG from cwSurvey2DGeometryRule");

    // 5) Paint the lines and station markers into the SVG
    QPainter painter(&generator);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(Qt::black);
    pen.setWidthF(0.1);
    painter.setPen(pen);

    // draw each shot line
    for (auto const& line : geometry.shotLines) {
        painter.drawLine(flip(line.p1()), flip(line.p2()));
    }

    // draw each station as a small circle + label
    QFont font = painter.font();
    font.setPointSizeF(0.8);
    painter.setFont(font);
    for (auto const& station : geometry.stations) {
        painter.drawEllipse(flip(station.position), 0.1, 0.1);
        painter.drawText(flip(station.position + QPointF(0.6, 0.2)),
                         station.name);
    }
    painter.end();

    // 6) Verify that the file was created
    REQUIRE(QFileInfo(svgPath).exists());
    // qDebug() << "SVG:" << svgPath;
}
