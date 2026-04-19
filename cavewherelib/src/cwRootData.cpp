/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwRegionTreeModel.h"
#include "cwCavingRegion.h"
#include "cwLinePlotManager.h"
#include "cwScrapManager.h"
#include "cwProject.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwSurveyExportManager.h"
#include "cwSurveyImportManager.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordFilterPipelineModel.h"
#include "cwQMLReload.h"
#include "cwLicenseAgreement.h"
#include "cwRegionSceneManager.h"
#include "cwTaskManagerModel.h"
#include "cwPageSelectionModel.h"
#include "cwSettings.h"
#include "cwRemoteServices.h"
#include "cwDeepLinkHandler.h"
// #include "cwImageCompressionUpdater.h"
#include "cwJobSettings.h"
#include "cwSurveyNoteModel.h"

//Qt includes
#include <QItemSelectionModel>
#include <QUndoStack>
#include <QSettings>
#include <QStandardPaths>
#include <QApplication>
#include <QStyle>
#include <QProcess>
#include <QDesktopServices>
#include <QClipboard>
#include <QFileInfo>

//Generated files from qbs
#include "cavewhereVersion.h"

//QQuickGit
#include "GitRepository.h"
#include "LfsBatchClient.h"

//QuickQanave includes
#include <QuickQanava>

cwRootData::cwRootData(QObject *parent) :
    QObject(parent),
    m_account(new QQuickGit::Account(this)),
    m_accountWatcher(new QQuickGit::AccountSettingWatcher(this)),
    m_recentProjectModel(new cwRecentProjectModel(this)),
    DefaultTrip(new cwTrip(this)),
    DefaultTripCalibration(new cwTripCalibration(this))
{
    cwSettings::initialize(); //Init's a singleton

    //Task Manager, allows the users to see running tasks
    TaskManagerModel = new cwTaskManagerModel(this);
    FutureManagerModel = new cwFutureManagerModel(this);
    m_keywordItemModel = new cwKeywordItemModel(this);
    m_keywordFilterPipelineModel = new cwKeywordFilterPipelineModel(this);
    m_keywordFilterPipelineModel->setKeywordModel(m_keywordItemModel);

    //Create the project, this saves and load data
    Project = new cwProject(this);
    Project->setGitAccount(m_account);
    // Project->setTaskManager(TaskManagerModel);
    Project->setFutureManagerToken(FutureManagerModel);
    m_recentProjectModel->setProject(Project);
    // Auto-add to recent list on save (covers save-as path changes).
    // Not needed on loaded — all loadProject() callers already add to recents
    // explicitly with the user-facing path (which may differ from the internal
    // .cwproj path after legacy conversion).
    connect(Project, &cwProject::fileSaved, this, [this]() {
        const QString currentProjectPath = QFileInfo(Project->filename()).absoluteFilePath();
        if (currentProjectPath.isEmpty()) {
            return;
        }
        const auto addResult = m_recentProjectModel->addRepositoryFromProjectFile(QUrl::fromLocalFile(currentProjectPath));
        if (addResult.hasError()) {
            qWarning() << "Failed to add recent project:" << addResult.errorMessage();
        }
    });
    remote();
    Project->setAuthProvider(remote()->authProvider());
    connect(Project, &cwProject::authProviderCredentialsNeeded,
            remote(), &cwRemoteServices::ensureGitHubTokenLoaded);

    Region = Project->cavingRegion();
    Region->setUndoStack(undoStack());

    //Setup the region tree
    RegionTreeModel = new cwRegionTreeModel(Project);
    RegionTreeModel->setCavingRegion(Region);

    //Setup the loop closer
    LinePlotManager = new cwLinePlotManager(Project);
    LinePlotManager->setRegion(Region);
    LinePlotManager->setFutureManagerToken(FutureManagerModel->token());

    //Setup the scrap manager
    ScrapManager = new cwScrapManager(Project);
    ScrapManager->setProject(Project);
    ScrapManager->setRegionTreeModel(RegionTreeModel);
    ScrapManager->setLinePlotManager(LinePlotManager);
    ScrapManager->setFutureManagerToken(FutureManagerModel->token());
    ScrapManager->setKeywordItemModel(m_keywordItemModel);

    //Setup the note lidar manager
    NoteLiDARManager = new cwNoteLiDARManager(Project);
    NoteLiDARManager->setProject(Project);
    NoteLiDARManager->setLinePlotManager(LinePlotManager);
    NoteLiDARManager->setRegionTreeModel(RegionTreeModel);
    NoteLiDARManager->setFutureManagerToken(FutureManagerModel->token());
    NoteLiDARManager->setKeywordItemModel(m_keywordItemModel);

    //Setup the sketch manager (writes sketch thumbnails into the shared cache)
    SketchManager = new cwSketchManager(Project);
    SketchManager->setProject(Project);
    SketchManager->setRegionTreeModel(RegionTreeModel);

    //Setup the survey import manager
    SurveyImportManager = new cwSurveyImportManager(Project);
    SurveyImportManager->setCavingRegion(Region);
    SurveyImportManager->setUndoStack(undoStack());
    SurveyImportManager->setErrorModel(Project->errorModel());

    //Save account settings to QSetting
    m_accountWatcher->setPerson(m_account);

    QuickView = nullptr;

    //For debugging
    QMLReloader = new cwQMLReload(this);

    //For license agreement
    License = new cwLicenseAgreement(this);
    License->setVersion(version());

    RegionSceneManager = new cwRegionSceneManager(this);
    RegionSceneManager->setCavingRegion(Region);

    ScrapManager->setRenderScraps(RegionSceneManager->items()); //scraps());
    LinePlotManager->setRenderLinePlot(RegionSceneManager->linePlot());
    NoteLiDARManager->setRender(RegionSceneManager->items());

    PageSelectionModel = new cwPageSelectionModel(this);

    // cwImageCompressionUpdater* imageUpdater = new cwImageCompressionUpdater(this);
    // imageUpdater->setFutureToken(FutureManagerModel->token());
    // imageUpdater->setRegionTreeModel(RegionTreeModel);

    auto updateAutomaticUpdate = [this]()
    {
        bool autoUpdate = cwJobSettings::instance()->automaticUpdate();
        LinePlotManager->setAutomaticUpdate(autoUpdate);
        ScrapManager->setAutomaticUpdate(autoUpdate);
    };

    updateAutomaticUpdate();

    connect(cwJobSettings::instance(), &cwJobSettings::automaticUpdateChanged,
            this, updateAutomaticUpdate);

    connect(Project, &cwProject::filenameChanged, this, [this]() {
        // Reset the filter pipeline UI state when the project file changes.
        // Do NOT clear m_keywordItemModel here — scrap keyword items self-destruct
        // via cwKeywordItem::setObject()'s destroyed-signal connection when their
        // associated cwScrap objects are deleted on project unload.  Clearing the
        // item model while new scraps are already registered (bundle load completes
        // after scraps are inserted) permanently hides their carpeting because the
        // guard in addKeywordItemForScrap() prevents re-registration.
        m_keywordFilterPipelineModel->clear();
    });
}

cwRootData::~cwRootData()
{
    shutdownBlocking();
}

/**
Gets undoStack
*/
QUndoStack* cwRootData::undoStack() const {
    return Project->undoStack();
}

/**
Sets quickWindow
*/
void cwRootData::setQuickView(QQuickView* quickView) {
    if(QuickView != quickView) {
        QuickView = quickView;
        //        QMLReloader->setQuickView(QuickView);
        emit quickWindowChanged();
    }
}

/**
 * @brief cwRootData::version
 * @return
 *
 * Returns the current version of cavewhere
 */
QString cwRootData::version() const {
    return CavewhereVersion; //Automatically generated from qbs in cavewhereVersion.h
}

/**
* @brief cwRootData::setLeadsVisible
* @param leadsVisible
*
* This function is temperary, should be moved to a layer manager
*/
void cwRootData::setLeadsVisible(bool leadsVisible) {
    if(LeadsVisible != leadsVisible) {
        LeadsVisible = leadsVisible;
        emit leadsVisibleChanged();
    }
}

/**
*
*/
void cwRootData::setStationVisible(bool stationsVisible) {
    if(StationVisible != stationsVisible) {
        StationVisible = stationsVisible;
        emit stationsVisibleChanged();
    }
}

/**
* @brief cwRootData::lastDirectory
* @return Returns the last directory open by the file dialog. If the directory doesn't exist, this opens the desktopLocation
*/
QUrl cwRootData::lastDirectory() const {
    QSettings settings;
    QUrl dir = settings.value("LastDirectory", QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation))).toUrl();
    return dir;
}

/**
* @brief cwRootData::setLastDirectory
* @param lastDirectory - Set the last directory that was opened by the user. This is useful for
* propertly position the file dialogs
*/
void cwRootData::setLastDirectory(QUrl lastDirectory) {
    const QString dirPath = cwSaveLoad::lastDirectoryForProjectFile(lastDirectory.toLocalFile());
    QUrl dir = QUrl::fromLocalFile(dirPath);

    if(this->lastDirectory() != dir) {
        QSettings settings;
        settings.setValue("LastDirectory", dir);
        emit lastDirectoryChanged();
    }
}

cwSettings* cwRootData::settings() const {
    return cwSettings::instance();
}

void cwRootData::setPageView(cwPageView *value) {
    if (m_pageView != value) {
        m_pageView = value;
        emit pageViewChanged();
    }
}

QString cwRootData::cwRootData::supportImageFormats() const {
    return cwProject::supportedImageFormats();
}

QUrl cwRootData::cavewhereImageUrl(const cwImage& image, const QString& absolutePath) const
{
    return cwImageProvider::imageUrl(image, absolutePath);
}

void cwRootData::newProject()
{
    prepareForProjectSwitch();
    Project->newProject();
}

/**
 * @brief cwRootData::loadProject
 * @param filename - The path of the project file to load
 *
 * All code paths that load a project file should use this method to avoid
 * bugs like issue #369 where forgetting to clear history left old trip
 * pages accessible after loading a new project.
 */
void cwRootData::loadProject(const QString& filename)
{
    prepareForProjectSwitch();
    Project->loadFile(filename);
}

/**
 * Clears page history and navigates to the View page so stale pages from
 * the previous project aren't reachable via sidebar or back/forward.
 */
void cwRootData::prepareForProjectSwitch()
{
    auto clearedComponents = PageSelectionModel->clearHistory();
    if (m_pageView) {
        m_pageView->clearPageCacheFor(clearedComponents);
    }
    PageSelectionModel->gotoPageByName(nullptr, "View");

    // Clear the undo stack so old cave/trip/note/scrap objects held
    // alive by undo commands are destroyed before the new project is
    // loaded.  Without this, QML can find stale objects from the
    // previous project.
    if (auto* stack = undoStack()) {
        stack->clear();
    }
}

void cwRootData::discardChangesAndReload()
{
    auto currentFile = Project->filename();
    if (currentFile.isEmpty()) {
        return;
    }
    connect(Project, &cwProject::discardCompleted, this, [this, currentFile]() {
        loadProject(currentFile);
    }, Qt::SingleShotConnection);

    Project->discardChanges();
}

int cwRootData::titleBarHeight() const
{
    Q_ASSERT(dynamic_cast<QApplication*>(QApplication::instance()) != nullptr);
    return static_cast<QApplication*>(QApplication::instance())->style()->pixelMetric(QStyle::PM_TitleBarHeight);
}

void cwRootData::showInFolder(const QString &path) const
{
    QFileInfo info(path);
#if defined(Q_OS_WIN)
    QStringList args;
    if (!info.isDir())
        args << "/select,";
    args << QDir::toNativeSeparators(path);
    if (QProcess::startDetached("explorer", args))
        return;
#elif defined(Q_OS_MAC) && QT_CONFIG(process)
    QStringList args;
    args << "-e";
    args << "tell application \"Finder\"";
    args << "-e";
    args << "activate";
    args << "-e";
    args << "select POSIX file \"" + path + "\"";
    args << "-e";
    args << "end tell";
    args << "-e";
    args << "return";
    if (!QProcess::execute("/usr/bin/osascript", args))
        return;
#endif
    QDesktopServices::openUrl(QUrl::fromLocalFile(info.isDir()? path : info.path()));

}

void cwRootData::copyText(const QString &text) const
{
    auto clipboard = QGuiApplication::clipboard();
    clipboard->setText(text);

}

bool cwRootData::pathExists(const QString &path) const
{
    return QFileInfo::exists(path);
}

QString cwRootData::sanitizeFileName(const QString &name) const
{
    return cwSaveLoad::sanitizeFileName(name);
}

QString cwRootData::fileBaseName(const QString &path) const
{
    QFileInfo info(path);
    return info.baseName();
}

QString cwRootData::fileName(const QString &path) const
{
    QFileInfo info(path);
    return info.fileName();
}

bool cwRootData::mobileBuild() const
{
    return isMobileBuild();
}

//Don't inline this because it depends on private CW_MOBILE_BUILD define
bool cwRootData::isMobileBuild()
{
#ifdef CW_MOBILE_BUILD
    return true;
#else
    return false;
#endif
}



cwNoteLiDARManager *cwRootData::noteLiDARManager() const
{
    return NoteLiDARManager;
}

void cwRootData::shutdown()
{
    if (m_shuttingDown) {
        return;
    }
    m_shuttingDown = true;

    auto checkComplete = [this]() {
        if (!m_shutdownCompleted
            && taskManagerModel()->isIdle()
            && futureManagerModel()->isEmpty())
        {
            m_shutdownCompleted = true;
            emit shutdownComplete();
        }
    };

    connect(taskManagerModel(), &cwTaskManagerModel::becameIdle,
            this, checkComplete);
    connect(futureManagerModel(), &cwFutureManagerModel::allFinished,
            this, checkComplete);

    checkComplete();
}

void cwRootData::shutdownBlocking()
{
    taskManagerModel()->waitForTasks();
    futureManagerModel()->waitForFinished();
    project()->waitSaveToFinish();
}

//For windows static linkage
void cwRootData::initCavewherelib()
{
    QQuickGit::GitRepository::initGitEngine();

    // Set SURVEXLIB so cavern_run() can find message files (en.msg).
    // This must be set before any cavern_run() call.
    for (const QDir& dir : cwGlobals::survexPath()) {
        if (QFileInfo(dir.filePath("en.msg")).exists()) {
            qputenv("SURVEXLIB", dir.absolutePath().toUtf8());
            break;
        }
    }
}

cwRemoteServices* cwRootData::remote() const
{
    if (!m_remoteServices) {
        m_remoteServices = new cwRemoteServices(const_cast<cwRootData*>(this));
    }
    return m_remoteServices;
}

cwDeepLinkHandler* cwRootData::deepLinkHandler() const
{
    if (!m_deepLinkHandler) {
        m_deepLinkHandler = new cwDeepLinkHandler(const_cast<cwRootData*>(this));
    }
    return m_deepLinkHandler;
}
