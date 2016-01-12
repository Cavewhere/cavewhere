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
class QGLWidget;
class QUndoStack;
class QOpenGLContext;
class QQuickView;
class QQmlApplicationEngine;

//Our includes
#include "cwGlobals.h"
class cwRegionTreeModel;
class cwCavingRegion;
class cwLinePlotManager;
class cwScrapManager;
class cwProject;
class cwTripCalibration;
class cwTrip;
class cwSurveyExportManager;
class cwSurveyImportManager;
class cwItemSelectionModel;
class cwQMLReload;
class cwLicenseAgreement;
class cwRegionSceneManager;
class cwScreen;
class cwEventRecorderModel;
class cwTaskManagerModel;
class cwPageSelectionModel;

#ifndef CAVEWHERE_VERSION
#define CAVEWHERE_VERSION "Sauce-Release" //This is automaticaly update with qmake
#endif

class CAVEWHERE_LIB_EXPORT cwRootData : public QObject
{
    Q_OBJECT

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
    Q_PROPERTY(QScreen* primaryScreen READ primaryScreen CONSTANT)
    Q_PROPERTY(cwEventRecorderModel* eventRecorderModel READ eventRecorderModel CONSTANT)
    Q_PROPERTY(cwTaskManagerModel* taskManagerModel READ taskManagerModel CONSTANT)
    Q_PROPERTY(cwPageSelectionModel* pageSelectionModel READ pageSelectionModel CONSTANT)
    Q_PROPERTY(cwRegionTreeModel* regionTreeModel READ regionTreeModel CONSTANT)
    Q_PROPERTY(QUrl lastDirectory READ lastDirectory WRITE setLastDirectory NOTIFY lastDirectoryChanged)

    //Temporary properties that should be move to a view layer model
    Q_PROPERTY(bool leadsVisible READ leadsVisible WRITE setLeadsVisible NOTIFY leadsVisibleChanged)

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
    cwEventRecorderModel* eventRecorderModel() const;
    cwTaskManagerModel* taskManagerModel() const;
    cwPageSelectionModel* pageSelectionModel() const;
    cwRegionTreeModel* regionTreeModel() const;

    void setQuickView(QQuickView* quickView);

    //Default class, aren't used exept to prevent qml from complaining
    cwTrip* defaultTrip() const;
    cwTripCalibration* defaultTripCalibration() const;

    bool leadsVisible() const;
    void setLeadsVisible(bool leadsVisible);

    QUrl lastDirectory() const;
    void setLastDirectory(QUrl lastDirectory);

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
    cwEventRecorderModel* EventRecorderModel; //!<
    cwTaskManagerModel* TaskManagerModel; //!<
    cwPageSelectionModel* PageSelectionModel; //!<
    cwRegionTreeModel* RegionTreeModel; //!<

    //Default class, aren't used exept to prevent qml from complaining
    cwTrip* DefaultTrip;
    cwTripCalibration* DefaultTripCalibration;

    //Temperary property should be move layer
    bool LeadsVisible; //!<

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
* @brief cwRoot::eventRecorderModel
* @return Returns the event recorder model. Used to record events for debugging playback
*/
inline cwEventRecorderModel* cwRootData::eventRecorderModel() const {
    return EventRecorderModel;
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


#endif // CWGLOBALQMLDATA_H
