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

class cwGlobalQMLData : public QObject
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
    explicit cwGlobalQMLData(QObject *parent = 0);
    cwCavingRegion* region() const;
    cwRegionTreeModel* regionModel() const;
    QGLWidget* mainGLWidget() const;
    void setGLWidget(QGLWidget* mainGLWidget);
    cwLinePlotManager* linePlotManager() const;
    cwScrapManager* scrapManager() const;
    cwProject* project() const;

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

    //Default class, aren't used exept to prevent qml from complaining
    cwTrip* DefaultTrip; //!<
    cwTripCalibration* DefaultTripCalibration;
};

/**
Gets defaultTrip
*/
inline cwTrip* cwGlobalQMLData::defaultTrip() const {
    return DefaultTrip;
}
/**
Gets defaultTripCalibration
*/
inline cwTripCalibration* cwGlobalQMLData::defaultTripCalibration() const {
    return DefaultTripCalibration;
}

/**
Gets region
*/
inline cwCavingRegion* cwGlobalQMLData::region() const {
    return Region;
}

/**
Gets regionModel
*/
inline cwRegionTreeModel* cwGlobalQMLData::regionModel() const {
    return RegionTreeModel;
}
/**
Gets mainGLWidget
*/
inline QGLWidget* cwGlobalQMLData::mainGLWidget() const {
    return GLWidget;
}

/**
  Gets linePlotManager
  */
inline cwLinePlotManager* cwGlobalQMLData::linePlotManager() const {
    return LinePlotManager;
}
/**
  Gets scrapManager
  */
inline cwScrapManager* cwGlobalQMLData::scrapManager() const {
    return ScrapManager;
}

/**
  Gets project
  */
inline cwProject* cwGlobalQMLData::project() const {
    return Project;
}
#endif // CWGLOBALQMLDATA_H
