#include "RootData.h"

//CaveWhere includes
#include "cwCave.h"
#include "cwSurveyDataArtifact.h"
#include "cwSurveyNetworkBuilderRule.h"
#include "cwSurvey2DGeometryRule.h"
#include "cwMatrix4x4Artifact.h"
#include "PenLineModelSerializer.h"

//Qt includes
#include <QScreen>
#include <QGuiApplication>
#include <QSurfaceFormat>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDir>

using namespace cwSketch;

RootData::RootData(QObject *parent) :
    m_project(new cwProject(this))
{
    //For testing
    m_centerlinePainterModel = new CenterlinePainterModel(this);
    m_penLineModel = new PenLineModel(this);
    createCurrentTrip();
    createGeometry2DPipeline();


    // 1) Ask Qt where the desktop folder is
    QString desktopDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QDir dir;
    dir.mkdir(desktopDir);

    // 2) Build the full path to your JSON file
    QString testPath = desktopDir + QDir::separator() + "testPen.json";

    if(QFile::exists(testPath)) {
        QFile file(testPath);
        file.open(QFile::ReadOnly);
        auto data = file.readAll();
        file.close();
        auto results = PenLineModelSerializer::deserialize(data);
        qDebug() << "Results:" << results.lines.size() << "error string:" << results.errorString << "data:" << data.size();
        m_penLineModel->setLines(results.lines);
    }

    auto savePenModel = [this, testPath]() {
        auto data = PenLineModelSerializer::serialize(*m_penLineModel);
        qDebug() << "TestPAth:" << testPath;
        QSaveFile saveFile(testPath);
        saveFile.open(QFile::WriteOnly);
        saveFile.write(data);
        saveFile.commit();
    };

    // connect(m_penLineModel, &PenLineModel::rowsInserted, this, savePenModel);
    // connect(m_penLineModel, &PenLineModel::rowsRemoved, this, savePenModel);
    connect(m_penLineModel, &PenLineModel::modelReset, this, savePenModel);

}

//For testing
void RootData::createCurrentTrip()
{

    // m_project->loadFile("/Users/cave/Desktop/nimbus.cw");
    // m_project->waitLoadToFinish();

    // qDebug() << "Caves:" << m_project->cavingRegion()->caveCount();

    // m_currentTrip = m_project->cavingRegion()->cave(0)->trip(1);

    cwTrip* trip = new cwTrip();
    trip->setName("My Trip");

    trip->addNewChunk();

    cwCave* cave = new cwCave();
    cave->setName("Test");

    cave->addTrip(trip);
    cavingRegion()->addCave(cave);

    m_currentTrip = trip;
}

//For testing
void RootData::createGeometry2DPipeline()
{
    // Create a survey data artifact and set the region
    cwSurveyDataArtifact* surveyData = new cwSurveyDataArtifact(this);
    surveyData->setRegion(cavingRegion());

    cwSurveyNetworkBuilderRule* networkBuilderRule = new cwSurveyNetworkBuilderRule(this);
    networkBuilderRule->setSurveyData(surveyData);

    cwSurvey2DGeometryRule* geometryRule = new cwSurvey2DGeometryRule(this);
    auto matrixArtifact = new cwMatrix4x4Artifact(geometryRule);

    //1:250 scales
    QScreen *screen = QGuiApplication::primaryScreen();
    qDebug() << "Device pixel ratio: " << screen->physicalDotsPerInch();
    qDebug() << "FIXME! geometry pipeline is still in testing mode!!!";


    //1:250
    const double footToMeter = 0.3048;
    // const double footToMeter = 1.0; //Operate in feet
    const double meterToInch = 1.0 / footToMeter * 12.0;

    const double mapScale = 250.0; //1:250

    //Map in inches
    double toMapInches = meterToInch/mapScale;

    //To pixels on screen
    double toScreen = toMapInches * screen->physicalDotsPerInch();

    QMatrix4x4 scaleMatrix;
    scaleMatrix.scale(toScreen);

    //Qt, postive y is down.
    QMatrix4x4 flipMatrix;
    scaleMatrix.scale(1.0, -1.0, 1.0);

    matrixArtifact->setMatrix4x4(scaleMatrix * flipMatrix);

    geometryRule->setSurveyNetwork(networkBuilderRule->surveyNetworkArtifact());
    geometryRule->setViewMatrix(matrixArtifact);

    //This is why you artifacts, it because you can interchange rules and the connections
    //still work.
    m_centerlinePainterModel->setSurvey2DGeometry(geometryRule->survey2DGeometry());
}

int RootData::sampleCount() const
{
    return QSurfaceFormat::defaultFormat().samples();
}


