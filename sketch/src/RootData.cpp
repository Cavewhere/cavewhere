#include "RootData.h"

//CaveWhere includes
#include "cwCave.h"
#include "PenLineModelSerializer.h"
#include "GitRepository.h"

//Qt includes
#include <QSurfaceFormat>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDir>

using namespace cwSketch;

RootData::RootData(QObject *parent) :
    m_repositoryModel(new RepositoryModel(this)),
    m_project(new cwProject(this)),
    m_account(new QQuickGit::Account(this)),
    m_accountWatcher(new QQuickGit::AccountSettingWatcher(this))
{
    //Save account settings to QSetting
    m_accountWatcher->setPerson(m_account);

    //For testing
    m_centerlinePainterModel = new CenterlinePainterModel(this);
    m_penLineModel = new PenLineModel(this);
    createCurrentTrip();

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

    // qDebug() << "Public key:";
    // qDebug() << m_account->publicKey();

    // QDir cloneDir(desktopDir + "/test");
    // cloneDir.removeRecursively();

    // qDebug() << "Clone dir:" << cloneDir;

    // QQuickGit::GitRepository repository;
    // repository.setDirectory(cloneDir);
    // auto future = repository.clone(QUrl("ssh://git@github.com/cavewhere/cavewhere-sketch.git"));


    // auto watcher = AsyncFuture::observe(future).context(this, [cloneDir, future]() {
    //     qDebug() << "Done checking out:";
    //     qDebug() << "Future error:" << future.result().errorMessage();
    //     qDebug() << "Files:" << cloneDir.entryList();
    //                             }).future();

    // AsyncFuture::waitForFinished(watcher);
    // qDebug() << "Future finished!" << future.result().hasError();
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

int RootData::sampleCount() const
{
    return QSurfaceFormat::defaultFormat().samples();
}



QQuickGit::Account *RootData::account() const
{
    return m_account;
}

RepositoryModel *RootData::repositoryModel() const
{
    return m_repositoryModel;
}
