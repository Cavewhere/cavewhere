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

class cwRootData : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwCavingRegion* region READ region NOTIFY regionChanged)
    Q_PROPERTY(cwRegionTreeModel* regionModel READ regionModel NOTIFY regionModelChanged)
    Q_PROPERTY(cwItemSelectionModel* regionSelectionModel READ regionSelectionModel)
    Q_PROPERTY(cwLinePlotManager* linePlotManager READ linePlotManager NOTIFY linePlotManagerChanged)
    Q_PROPERTY(cwScrapManager* scrapManager READ scrapManager NOTIFY scrapManagerChanged)
    Q_PROPERTY(cwProject* project READ project NOTIFY projectChanged)
    Q_PROPERTY(cwTripCalibration* defaultTripCalibration READ defaultTripCalibration NOTIFY defaultTripCalibrationChanged)
    Q_PROPERTY(cwTrip* defaultTrip READ defaultTrip NOTIFY defaultTripChanged)
    Q_PROPERTY(cwSurveyExportManager* surveyExportManager READ surveyExportManager NOTIFY surveyExportManagerChanged)
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

public:
    explicit cwRootData(QObject *parent = 0);
    cwCavingRegion* region() const;
    cwRegionTreeModel* regionModel() const;
    cwItemSelectionModel* regionSelectionModel() const;
    cwLinePlotManager* linePlotManager() const;
    cwScrapManager* scrapManager() const;
    cwProject* project() const;
    cwSurveyExportManager* surveyExportManager() const;
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

    void setQuickView(QQuickView* quickView);

    //Default class, aren't used exept to prevent qml from complaining
    cwTrip* defaultTrip() const;
    cwTripCalibration* defaultTripCalibration() const;

signals:
    void regionChanged();
    void regionModelChanged();
    void linePlotManagerChanged();
    void scrapManagerChanged();
    void projectChanged();
    void surveyExportManagerChanged();
    void surveyImportManagerChanged();
    void undoStackChanged();
    void quickWindowChanged();
    void qmlReloaderChanged();
    void defaultTripChanged();
    void defaultTripCalibrationChanged();
    void versionChanged();
    void licenseChanged();
    void regionSceneManagerChanged();

public slots:

private:
    cwCavingRegion* Region; //!< Where all the data is stored
    cwRegionTreeModel* RegionTreeModel; //!< For so listviews can access Region
    cwItemSelectionModel* RegionTreeSelectionModel; //!< Allows survey export manager to know what item's are selected
    cwLinePlotManager* LinePlotManager; //!< For keeping the lineplot updated
    cwScrapManager* ScrapManager; //!< For keeping all the scraps updated (carpeting)
    cwProject* Project; //!< For saving and loading, image saving and loading
    cwSurveyExportManager* SurveyExportManager; //!< For export survey data to compass, survex, etc
    cwSurveyImportManager* SurveyImportManager; //!< For importing survey data from survex, etc
    QQuickView* QuickView; //!< For exporting the 3d screen to a file
    cwQMLReload* QMLReloader; //!< For reloading the QML data on the fly
    cwLicenseAgreement* License; //!<
    cwRegionSceneManager* RegionSceneManager; //!<
    cwEventRecorderModel* EventRecorderModel; //!<
    cwTaskManagerModel* TaskManagerModel; //!<
    cwPageSelectionModel* PageSelectionModel; //!<


    //Default class, aren't used exept to prevent qml from complaining
    cwTrip* DefaultTrip;
    cwTripCalibration* DefaultTripCalibration;
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
Gets regionModel
*/
inline cwRegionTreeModel* cwRootData::regionModel() const {
    return RegionTreeModel;
}

/**
  Get the region tree selection model

  This allow the cpp to know when the user selects different items (region, cave, trip)
  in the data section
  */
inline cwItemSelectionModel *cwRootData::regionSelectionModel() const {
    return RegionTreeSelectionModel;
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
  Get's the survey export manager, this holds the menu actions for exporting survey data
  */
inline cwSurveyExportManager *cwRootData::surveyExportManager() const
{
    return SurveyExportManager;
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



#endif // CWGLOBALQMLDATA_H
