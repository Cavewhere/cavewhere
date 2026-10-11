/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Qt includes
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QThreadPool>

//Our includes
#include "cwGlobals.h"
#include "cwMetaTypeSystem.h"
#include "cwRootData.h"
#include "cwSettings.h"
#include "cwTask.h"

//QQuickGit includes
#include "GitRepository.h"

namespace {

constexpr int kWindowSamples = 4;
constexpr int kDefaultJunctions = 3;
constexpr int kNoSeed = -1;

} // namespace

int main(int argc, char* argv[])
{
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QQuickStyle::setStyle(QStringLiteral("CaveWhereStyle"));
    QQuickStyle::setFallbackStyle(QStringLiteral("Fusion"));

    QApplication::setOrganizationName(QStringLiteral("Vadose Solutions"));
    QApplication::setOrganizationDomain(QStringLiteral("cavewhere.com"));
    QApplication::setApplicationName(QStringLiteral("cavewhere-tube-gallery"));

    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Renders the passage tube junction catalog for checking by eye."));
    parser.addHelpOption();
    const QCommandLineOption caseOption(QStringLiteral("case"),
                                        QStringLiteral("Open on the case with this name, or the first whose name starts with it."),
                                        QStringLiteral("name"));
    const QCommandLineOption seedOption(QStringLiteral("seed"),
                                        QStringLiteral("Open on a random survey from this seed."),
                                        QStringLiteral("n"));
    const QCommandLineOption junctionsOption(QStringLiteral("junctions"),
                                             QStringLiteral("Junctions in the random survey (default 3)."),
                                             QStringLiteral("k"));
    const QCommandLineOption grabOption(QStringLiteral("grab"),
                                        QStringLiteral("Write the four preset screenshots of the opened case to dir, then exit."),
                                        QStringLiteral("dir"));
    const QCommandLineOption grabAllOption(QStringLiteral("grab-all"),
                                           QStringLiteral("Write the preset screenshots of every case to dir, then exit."),
                                           QStringLiteral("dir"));
    const QCommandLineOption failingOption(QStringLiteral("failing"),
                                           QStringLiteral("With --grab-all, only the cases with mesh issues."));
    const QCommandLineOption sweepOption(QStringLiteral("sweep"),
                                         QStringLiteral("Include the parametric sweep cases in the list."));
    parser.addOptions({caseOption, seedOption, junctionsOption, grabOption, grabAllOption, failingOption, sweepOption});
    parser.process(app);

    cwMetaTypeSystem::registerTypes();
    cwGlobals::loadFonts();
    cwSettings::initialize();

    QSurfaceFormat format;
    format.setSamples(kWindowSamples);
    QSurfaceFormat::setDefaultFormat(format);

    auto* engine = new QQmlApplicationEngine();
    cwRootData::initCavewherelib();

    const bool grabAll = parser.isSet(grabAllOption);
    const QString grabDirectory = grabAll ? parser.value(grabAllOption) : parser.value(grabOption);
    engine->setInitialProperties({
        {QStringLiteral("initialCase"), parser.value(caseOption)},
        {QStringLiteral("randomSeed"), parser.isSet(seedOption) ? parser.value(seedOption).toInt() : kNoSeed},
        {QStringLiteral("randomJunctions"), parser.isSet(junctionsOption) ? parser.value(junctionsOption).toInt() : kDefaultJunctions},
        {QStringLiteral("grabDirectory"), grabDirectory},
        {QStringLiteral("grabAll"), grabAll},
        {QStringLiteral("failingOnly"), parser.isSet(failingOption)},
        {QStringLiteral("includeSweep"), parser.isSet(sweepOption)},
        {QStringLiteral("defaultGrabDirectory"), QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("tube-shots"))},
    });

    QObject::connect(engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    QObject::connect(engine, &QQmlEngine::quit, &app, &QCoreApplication::quit, Qt::QueuedConnection);
    QObject::connect(engine, &QQmlEngine::exit, &app, &QCoreApplication::exit, Qt::QueuedConnection);

    engine->loadFromModule(QStringLiteral("cw.TubeGallery"), QStringLiteral("TubeGalleryWindow"));

    const int result = app.exec();
    delete engine;
    QThreadPool::globalInstance()->waitForDone();
    cwTask::threadPool()->waitForDone();
    QQuickGit::GitRepository::shutdownGitEngine();
    return result;
}
