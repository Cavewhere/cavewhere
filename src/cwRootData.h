#ifndef CWGLOBALQMLDATA_H
#define CWGLOBALQMLDATA_H

//Qt includes
#include <QObject>
class QGLWidget;
class QUndoStack;

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

class cwRootData : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwCavingRegion* region READ region NOTIFY regionChanged)
    Q_PROPERTY(cwRegionTreeModel* regionModel READ regionModel NOTIFY regionModelChanged)
    Q_PROPERTY(cwItemSelectionModel* regionSelectionModel READ regionSelectionModel)
    Q_PROPERTY(QGLWidget* mainGLWidget READ mainGLWidget WRITE setGLWidget NOTIFY mainGLWidgetChanged)
    Q_PROPERTY(cwLinePlotManager* linePlotManager READ linePlotManager NOTIFY linePlotManagerChanged)
    Q_PROPERTY(cwScrapManager* scrapManager READ scrapManager NOTIFY scrapManagerChanged)
    Q_PROPERTY(cwProject* project READ project NOTIFY projectChanged)
    Q_PROPERTY(cwTripCalibration* defaultTripCalibration READ defaultTripCalibration NOTIFY defaultTripCalibrationChanged)
    Q_PROPERTY(cwTrip* defaultTrip READ defaultTrip NOTIFY defaultTripChanged)
    Q_PROPERTY(cwSurveyExportManager* surveyExportManager READ surveyExportManager NOTIFY surveyExportManagerChanged)
    Q_PROPERTY(cwSurveyImportManager* surveyImportManager READ surveyImportManager NOTIFY surveyImportManagerChanged)
    Q_PROPERTY(QUndoStack* undoStack READ undoStack NOTIFY undoStackChanged)

public:
    explicit cwRootData(QObject *parent = 0);
    cwCavingRegion* region() const;
    cwRegionTreeModel* regionModel() const;
    cwItemSelectionModel* regionSelectionModel() const;
    QGLWidget* mainGLWidget() const;
    void setGLWidget(QGLWidget* mainGLWidget);
    cwLinePlotManager* linePlotManager() const;
    cwScrapManager* scrapManager() const;
    cwProject* project() const;
    cwSurveyExportManager* surveyExportManager() const;
    cwSurveyImportManager* surveyImportManager() const;
    QUndoStack* undoStack() const;

    //Default class, aren't used exept to prevent qml from complaining
    cwTrip* defaultTrip() const;
    cwTripCalibration* defaultTripCalibration() const;

signals:
    void regionChanged();
    void regionModelChanged();
    void mainGLWidgetChanged();
    void linePlotManagerChanged();
    void scrapManagerChanged();
    void projectChanged();
    void surveyExportManagerChanged();
    void surveyImportManagerChanged();
    void undoStackChanged();

    void defaultTripChanged();
    void defaultTripCalibrationChanged();


public slots:

private:
    cwCavingRegion* Region; //!< Where all the data is stored
    cwRegionTreeModel* RegionTreeModel; //!< For so listviews can access Region
    cwItemSelectionModel* RegionTreeSelectionModel; //!< Allows survey export manager to know what item's are selected
    QGLWidget* GLWidget; //!< For 3d rendering
    cwLinePlotManager* LinePlotManager; //!< For keeping the lineplot updated
    cwScrapManager* ScrapManager; //!< For keeping all the scraps updated (carpeting)
    cwProject* Project; //!< For saving and loading, image saving and loading
    cwSurveyExportManager* SurveyExportManager; //!< For export survey data to compass, survex, etc
    cwSurveyImportManager* SurveyImportManager; //!< For importing survey data from survex, etc
    QUndoStack* UndoStack; //!< For Undo / redo support

    //Default class, aren't used exept to prevent qml from complaining
    cwTrip* DefaultTrip;
    cwTripCalibration* DefaultTripCalibration;
};

/**
Gets undoStack
*/
inline QUndoStack* cwRootData::undoStack() const {
    return UndoStack;
}

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
Gets mainGLWidget
*/
inline QGLWidget* cwRootData::mainGLWidget() const {
    return GLWidget;
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

#endif // CWGLOBALQMLDATA_H
