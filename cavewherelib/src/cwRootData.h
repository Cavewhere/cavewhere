/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLOBALQMLDATA_H
#define CWGLOBALQMLDATA_H

//Qt includes
#include <QObject>
#include <QGuiApplication>
#include <QUndoStack>
#include <QOpenGLContext>
#include <QQuickView>
#include <QQmlApplicationEngine>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"
#include "cwRegionTreeModel.h"
#include "cwCavingRegion.h"
#include "cwLinePlotManager.h"
#include "cwScrapManager.h"
#include "cwProject.h"
#include "cwTripCalibration.h"
#include "cwTrip.h"
#include "cwSurveyExportManager.h"
#include "cwSurveyImportManager.h"
#include "cwQMLReload.h"
#include "cwLicenseAgreement.h"
#include "cwRegionSceneManager.h"
#include "cwTaskManagerModel.h"
#include "cwFutureManagerModel.h"
#include "cwPageSelectionModel.h"
#include "cwSettings.h"
#include "cwPageView.h"

#ifndef CAVEWHERE_VERSION
#define CAVEWHERE_VERSION "Sauce-Release" //This is automaticaly update with qmake
#endif

class CAVEWHERE_LIB_EXPORT cwRootData : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(RootData)

    Q_PROPERTY(cwCavingRegion* region READ region NOTIFY regionChanged)
    Q_PROPERTY(cwLinePlotManager* linePlotManager READ linePlotManager NOTIFY linePlotManagerChanged)
    Q_PROPERTY(cwScrapManager* scrapManager READ scrapManager NOTIFY scrapManagerChanged)
    Q_PROPERTY(cwProject* project READ project NOTIFY projectChanged)
    Q_PROPERTY(cwTripCalibration* defaultTripCalibration READ defaultTripCalibration NOTIFY defaultTripCalibrationChanged)
    Q_PROPERTY(cwTrip* defaultTrip READ defaultTrip NOTIFY defaultTripChanged)
    Q_PROPERTY(cwSurveyImportManager* surveyImportManager READ surveyImportManager NOTIFY surveyImportManagerChanged)
    Q_PROPERTY(QUndoStack* undoStack READ undoStack NOTIFY undoStackChanged)
    Q_PROPERTY(QQuickView* quickView READ quickView WRITE setQuickView NOTIFY quickWindowChanged)
    Q_PROPERTY(cwQMLReload* qmlReloader READ qmlReloader NOTIFY qmlReloaderChanged)
    Q_PROPERTY(QString version READ version NOTIFY versionChanged)
    Q_PROPERTY(cwLicenseAgreement* license READ license NOTIFY licenseChanged)
    Q_PROPERTY(cwRegionSceneManager* regionSceneManager READ regionSceneManager NOTIFY regionSceneManagerChanged)
    Q_PROPERTY(cwTaskManagerModel* taskManagerModel READ taskManagerModel CONSTANT)
    Q_PROPERTY(cwFutureManagerModel* futureManagerModel READ futureManagerModel CONSTANT)

    Q_PROPERTY(cwPageSelectionModel* pageSelectionModel READ pageSelectionModel CONSTANT)
    Q_PROPERTY(cwPageView* pageView READ pageView WRITE setPageView NOTIFY pageViewChanged)

    Q_PROPERTY(cwRegionTreeModel* regionTreeModel READ regionTreeModel CONSTANT)

    //Settings
    Q_PROPERTY(QUrl lastDirectory READ lastDirectory WRITE setLastDirectory NOTIFY lastDirectoryChanged)
    Q_PROPERTY(cwSettings* settings READ settings CONSTANT)
    Q_PROPERTY(QString supportImageFormats READ supportImageFormats CONSTANT)

    Q_PROPERTY(int titleBarHeight READ titleBarHeight CONSTANT)

    //Temporary properties that should be move to a view layer model
    Q_PROPERTY(bool leadsVisible READ leadsVisible WRITE setLeadsVisible NOTIFY leadsVisibleChanged)
    Q_PROPERTY(bool stationsVisible READ stationsVisible WRITE setStationVisible NOTIFY stationsVisibleChanged)

public:
    explicit cwRootData(QObject *parent = 0);
    cwCavingRegion* region() const;
    cwLinePlotManager* linePlotManager() const;
    cwScrapManager* scrapManager() const;
    cwProject* project() const;
    cwSurveyImportManager* surveyImportManager() const;
    QUndoStack* undoStack() const;
    QQuickView* quickView() const;
    cwQMLReload* qmlReloader() const;
    QString version() const;
    cwLicenseAgreement* license() const;
    cwRegionSceneManager* regionSceneManager() const;
    QScreen* primaryScreen() const;
    cwTaskManagerModel* taskManagerModel() const;
    cwFutureManagerModel* futureManagerModel() const;
    cwPageSelectionModel* pageSelectionModel() const;
    cwRegionTreeModel* regionTreeModel() const;
    cwSettings* settings() const;

    cwPageView* pageView() const { return m_pageView; }
    void setPageView(cwPageView* value);

    void setQuickView(QQuickView* quickView);

    //Default class, aren't used exept to prevent qml from complaining
    cwTrip* defaultTrip() const;
    cwTripCalibration* defaultTripCalibration() const;

    bool leadsVisible() const;
    void setLeadsVisible(bool leadsVisible);

    bool stationsVisible() const;
    void setStationVisible(bool stationsVisible);

    QUrl lastDirectory() const;
    void setLastDirectory(QUrl lastDirectory);

    QString supportImageFormats() const;

    //Helper functions for creating things
    Q_INVOKABLE cwImage emptyImage() const  { return cwImage(); }
    Q_INVOKABLE QUrl cavewhereImageUrl(int id) const;
    Q_INVOKABLE void newProject();

    int titleBarHeight() const;

    //Utils
    Q_INVOKABLE void showInFolder(const QString& path) const;
    Q_INVOKABLE void copyText(const QString& text) const;

signals:
    void regionChanged();
    void linePlotManagerChanged();
    void scrapManagerChanged();
    void projectChanged();
    void surveyImportManagerChanged();
    void undoStackChanged();
    void quickWindowChanged();
    void qmlReloaderChanged();
    void defaultTripChanged();
    void defaultTripCalibrationChanged();
    void versionChanged();
    void licenseChanged();
    void regionSceneManagerChanged();
    void leadsVisibleChanged();
    void lastDirectoryChanged();
    void stationsVisibleChanged();
    void pageViewChanged();

public slots:

private:
    cwCavingRegion* Region; //!< Where all the data is stored
    cwLinePlotManager* LinePlotManager; //!< For keeping the lineplot updated
    cwScrapManager* ScrapManager; //!< For keeping all the scraps updated (carpeting)
    cwProject* Project; //!< For saving and loading, image saving and loading
    cwSurveyImportManager* SurveyImportManager; //!< For importing survey data from survex, etc
    QQuickView* QuickView; //!< For exporting the 3d screen to a file
    cwQMLReload* QMLReloader; //!< For reloading the QML data on the fly
    cwLicenseAgreement* License; //!<
    cwRegionSceneManager* RegionSceneManager; //!<
    cwTaskManagerModel* TaskManagerModel; //!<
    cwFutureManagerModel* FutureManagerModel; //!<
    cwPageSelectionModel* PageSelectionModel; //!<
    cwRegionTreeModel* RegionTreeModel; //!<

    QPointer<cwPageView> m_pageView;

    //Default class, aren't used exept to prevent qml from complaining
    cwTrip* DefaultTrip;
    cwTripCalibration* DefaultTripCalibration;

    //Temperary property should be move layer
    bool LeadsVisible = true; //!<
    bool StationVisible = true; //!<

    int m_titleBarHeight;
};

/**
Gets defaultTrip
*/
inline cwTrip* cwRootData::defaultTrip() const {
    return DefaultTrip;
}
/**
Gets defaultTripCalibration
*/
inline cwTripCalibration* cwRootData::defaultTripCalibration() const {
    return DefaultTripCalibration;
}

/**
Gets region
*/
inline cwCavingRegion* cwRootData::region() const {
    return Region;
}

/**
  Gets linePlotManager
  */
inline cwLinePlotManager* cwRootData::linePlotManager() const {
    return LinePlotManager;
}
/**
  Gets scrapManager
  */
inline cwScrapManager* cwRootData::scrapManager() const {
    return ScrapManager;
}

/**
  Gets project
  */
inline cwProject* cwRootData::project() const {
    return Project;
}

/**
  Gets surveyImportManager
  */
inline cwSurveyImportManager* cwRootData::surveyImportManager() const {
    return SurveyImportManager;
}

/**
Gets quickWindow
*/
inline QQuickView* cwRootData::quickView() const {
    return QuickView;
}

/**
Gets qmlReloader
*/

inline cwQMLReload* cwRootData::qmlReloader() const {
    return QMLReloader;
}


/**
Gets license
*/
inline cwLicenseAgreement* cwRootData::license() const {
    return License;
}

/**
* @brief cwRootData::regionSceneManager
* @return
*/
inline cwRegionSceneManager* cwRootData::regionSceneManager() const {
    return RegionSceneManager;
}


/**
* @brief class::primaryScreen
* @return
*/
inline QScreen* cwRootData::primaryScreen() const {
    return QGuiApplication::primaryScreen();
}

/**
* @brief cwRootData::taskManagerModel
* @return The task manager model for visualizing running tasks
*/
inline cwTaskManagerModel* cwRootData::taskManagerModel() const {
    return TaskManagerModel;
}

/**
* @brief cwRootData::pageSelectionModel
* @return
*/
inline cwPageSelectionModel* cwRootData::pageSelectionModel() const {
    return PageSelectionModel;
}

/**
* @brief cwRootD::regionTreeModel
* @return
*/
inline cwRegionTreeModel* cwRootData::regionTreeModel() const {
    return RegionTreeModel;
}

/**
* @brief cwRootData::leadsVisible
* @return The visiblity of the leads - temporay, should be moved to layer manager
*/
inline bool cwRootData::leadsVisible() const {
    return LeadsVisible;
}

/**
* Returns true if station labels are visible, otherwise false and invisilbe
*/
inline bool cwRootData::stationsVisible() const {
    return StationVisible;
}

/**
* Returns the current future manager
*/
inline cwFutureManagerModel* cwRootData::futureManagerModel() const {
    return FutureManagerModel;
}

/**
*
*/


#endif // CWGLOBALQMLDATA_H
