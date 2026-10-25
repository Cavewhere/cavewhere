//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFutureManagerModel.h"
#include "cwJobSettings.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"
#include "ProfileLogCapture.h"
#include "TestHelper.h"

//Qt includes
#include <QDir>
#include <QRegularExpression>
#include <QTemporaryDir>

namespace {
QStringList projectLines()
{
    return ProfileLogCapture::linesStartingWith(QStringLiteral("project "));
}

QRegularExpression phasePattern(const QString& phase)
{
    return QRegularExpression(QStringLiteral("^project %1 us=\\d+$").arg(phase));
}

void addSurvey(cwCavingRegion* region)
{
    region->addCave();
    cwCave* cave = region->cave(0);
    cave->setName(QStringLiteral("ProfileCave"));
    cave->addTrip();
    cwTrip* trip = cave->trip(0);
    trip->setName(QStringLiteral("ProfileTrip"));

    cwShot shot;
    shot.setDistance(cwDistanceReading(QStringLiteral("10")));
    shot.setCompass(cwCompassReading(QStringLiteral("0")));
    shot.setBackCompass(cwCompassReading(QStringLiteral("180")));
    shot.setClino(cwClinoReading(QStringLiteral("0")));
    shot.setBackClino(cwClinoReading(QStringLiteral("0")));

    cwStation from;
    from.setName(QStringLiteral("a1"));
    cwStation to;
    to.setName(QStringLiteral("a2"));

    auto* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    chunk->appendShot(from, to, shot);
}
} // namespace

TEST_CASE("Opening a native project writes one cw.profile.load line per load phase",
          "[SaveLoad][loadProfile]")
{
    cwJobSettings::initialize();

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("LoadProfile.cwproj"));

    QString projectFile;
    {
        auto authorRoot = std::make_unique<cwRootData>();
        addSurvey(authorRoot->project()->cavingRegion());
        REQUIRE(authorRoot->project()->saveAs(projectPath));
        authorRoot->futureManagerModel()->waitForFinished();
        authorRoot->project()->waitSaveToFinish();
        projectFile = authorRoot->project()->filename();
    }
    REQUIRE(QFileInfo::exists(projectFile));

    const ProfileLogCapture capture(QStringLiteral("cw.profile.load.debug=true"));

    auto readerRoot = std::make_unique<cwRootData>();
    readerRoot->project()->loadOrConvert(projectFile);
    readerRoot->project()->waitLoadToFinish();
    REQUIRE(readerRoot->region()->caveCount() == 1);

    // The solve kick is queued behind the load continuation.
    CHECK(waitUntil([]() {
        return !projectLines().filter(phasePattern(QStringLiteral("solveKick"))).isEmpty();
    }));
    readerRoot->linePlotManager()->waitToFinish();

    const QStringList lines = projectLines();
    INFO("project lines: " << lines.join(QStringLiteral(" | ")).toStdString());

    CHECK(lines.filter(phasePattern(QStringLiteral("parse"))).size() == 1);
    CHECK(lines.filter(phasePattern(QStringLiteral("initializeRepository"))).size() == 1);
    CHECK(lines.filter(phasePattern(QStringLiteral("setData"))).size() == 1);

    const qsizetype parseIndex = lines.indexOf(phasePattern(QStringLiteral("parse")));
    const qsizetype initializeIndex = lines.indexOf(phasePattern(QStringLiteral("initializeRepository")));
    const qsizetype setDataIndex = lines.indexOf(phasePattern(QStringLiteral("setData")));
    const qsizetype solveKickIndex = lines.indexOf(phasePattern(QStringLiteral("solveKick")));
    REQUIRE(parseIndex >= 0);
    CHECK(parseIndex < initializeIndex);
    CHECK(initializeIndex < setDataIndex);
    CHECK(setDataIndex < solveKickIndex);

    // Opening the repository runs the sync-health status walk.
    const QRegularExpression gitStatusPattern(
        QStringLiteral("^project gitStatus us=\\d+ modified=\\d+$"));
    CHECK(!lines.filter(gitStatusPattern).isEmpty());
}
