/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Cavewhere includes
#include "cwCompassImporter.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwTeam.h"
#include "cwTeamMember.h"

//AsyncFuture includes
#include "asyncfuture.h"

//Qt includes
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QFile>
#include <QFuture>

namespace {

constexpr int kFormerTeamLimit = 100;

const QStringList& teamNames()
{
    static const QStringList names = {
        QStringLiteral("Philip Schuchardt"),
        QStringLiteral("Bob Thrun"),
        QStringLiteral("Larry Fish"),
        QStringLiteral("Dave Bunnell"),
        QStringLiteral("Scott Fee"),
        QStringLiteral("Roger Brucker"),
        QStringLiteral("Patricia Crowther"),
        QStringLiteral("Red Watson"),
        QStringLiteral("John Wilcox"),
        QStringLiteral("Richard Zopf"),
    };
    return names;
}

QString writeCompassFile(const QTemporaryDir& dir, const QString& team)
{
    const QString path = dir.filePath(QStringLiteral("teamLength.dat"));

    const QString contents = QStringLiteral(
        "Test Cave\n"
        "SURVEY NAME: A\n"
        "SURVEY DATE: 1 1 2024\n"
        "SURVEY TEAM:\n"
        "%1\n"
        "DECLINATION: 0.00  FORMAT: DDDDLRUDLAaDdNF  CORRECTIONS: 0.00 0.00 0.00\n"
        "\n"
        "FROM TO LENGTH BEARING INC LEFT UP DOWN RIGHT FLAGS COMMENTS\n"
        "\n"
        "A1 A2 16.70 323.90 30.40 1.00 1.00 1.00 1.00 #| #\n").arg(team);

    QFile file(path);
    REQUIRE(file.open(QFile::WriteOnly));
    file.write(contents.toLocal8Bit());
    file.close();

    return path;
}

} // namespace

TEST_CASE("Compass import keeps a survey team longer than 100 characters", "[Compass][CompassTeam]") {

    const QString team = teamNames().join(QStringLiteral(", "));
    REQUIRE(team.size() > kFormerTeamLimit);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString datFile = writeCompassFile(dir, team);

    auto future = cwCompassImporter::run(QStringList() << datFile);
    REQUIRE(AsyncFuture::waitForFinished(future));
    const auto result = future.result();

    for(const QString& message : result.messages) {
        INFO("Importer message: " << message.toStdString());
        CHECK_FALSE(message.contains(QStringLiteral("trimming"), Qt::CaseInsensitive));
        CHECK_FALSE(message.contains(QStringLiteral("100 characters")));
    }

    REQUIRE(result.caves.size() == 1);
    auto cave = std::make_unique<cwCave>();
    cave->setData(result.caves.first());

    REQUIRE(cave->trips().size() == 1);
    const cwTrip* trip = cave->trips().first();

    QStringList importedNames;
    const QList<cwTeamMember> members = trip->team()->teamMembers();
    for(const cwTeamMember& member : members) {
        importedNames.append(member.name());
    }

    CHECK(importedNames.join(QStringLiteral(", ")).toStdString() == team.toStdString());
}
