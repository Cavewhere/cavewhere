#ifndef CWGLOBALQMLDATA_H
#define CWGLOBALQMLDATA_H

//Qt includes
#include <QObject>
class QGLWidget;

//Our includes
class cwRegionTreeModel;
class cwCavingRegion;
class cwLinePlotManager;
class cwScrapManager;
class cwProject;
class cwTripCalibration;
class cwTrip;
class cwSurveyExportManager;

class cwRootData : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwCavingRegion* region READ region NOTIFY regionChanged)
    Q_PROPERTY(cwRegionTreeModel* regionModel READ regionModel NOTIFY regionModelChanged)
    Q_PROPERTY(QGLWidget* mainGLWidget READ mainGLWidget WRITE setGLWidget NOTIFY mainGLWidgetChanged)
    Q_PROPERTY(cwLinePlotManager* linePlotManager READ linePlotManager NOTIFY linePlotManagerChanged)
    Q_PROPERTY(cwScrapManager* scrapManager READ scrapManager NOTIFY scrapManagerChanged)
    Q_PROPERTY(cwProject* project READ project NOTIFY projectChanged)
    Q_PROPERTY(cwTripCalibration* defaultTripCalibration READ defaultTripCalibration NOTIFY defaultTripCalibrationChanged)
    Q_PROPERTY(cwTrip* defaultTrip READ defaultTrip NOTIFY defaultTripChanged)

public:
    explicit cwRootData(QObject *parent = 0);
    cwCavingRegion* region() const;
    cwRegionTreeModel* regionModel() const;
    QGLWidget* mainGLWidget() const;
    void setGLWidget(QGLWidget* mainGLWidget);
    cwLinePlotManager* linePlotManager() const;
    cwScrapManager* scrapManager() const;
    cwProject* project() const;
    cwSurveyExportManager* surveyExportManager() const;

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
    void defaultTripChanged();
    void defaultTripCalibrationChanged();

public slots:

private:
    cwCavingRegion* Region; //!< Where all the data is stored
    cwRegionTreeModel* RegionTreeModel; //!<
    QGLWidget* GLWidget; //!<
    cwLinePlotManager* LinePlotManager; //!<
    cwScrapManager* ScrapManager; //!<
    cwProject* Project; //!<
    cwSurveyExportManager* SurveyExportManager;

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
#endif // CWGLOBALQMLDATA_H
