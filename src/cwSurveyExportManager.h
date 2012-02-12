#ifndef CWSURVEYEXPORTMANGER_H
#define CWSURVEYEXPORTMANGER_H

//Qt includes
#include <QObject>
class QAction;
class QMenu;
class QItemSelectionModel;

//Our includes
class cwRegionTreeModel;

/**
    This class will open dialogs to export data.

    This class controls the different survey data exporters: survex and compass
  */
class cwSurveyExportManager : public QObject
{
    Q_OBJECT
public:
    explicit cwSurveyExportManager(QObject *parent = 0);
    
    QList<QMenu*> menus() const;

    cwRegionTreeModel* cavingRegionTreeModel() const;
    void setCavingRegionTreeModel(cwRegionTreeModel* model);

signals:
    
public slots:
    //Survex exports
    void openExportSurvexTripFileDialog();
    void openExportSurvexCaveFileDialog();
    void openExportSurvexRegionFileDialog();

    //Compass exports
    void openExportCompassTripFileDialog();
    void openExportCompassCaveFileDialog();
    void openExportCompassRegionFileDialog();

private slots:
    void exportSurvexTrip(QString filename);
    void exportSurvexCave(QString filename);
    void exportSurvexRegion(QString filename);
    void exportCaveToCompass(QString filename);
    void exporterFinished();

private:
    //This model allows use to export different data
    cwRegionTreeModel* Model;
    QItemSelectionModel* SelectionModel; //Query which item is selected


    //The menus that hold the actions
    QMenu* SurvexMenu;
    QMenu* CompassMenu;

    //The actions that allow the user to export data
    QAction* ExportSurvexTripAction;
    QAction* ExportSurvexCaveAction;
    QAction* ExportSurvexRegionAction;
    QAction* ExportCompassTripAction;
    QAction* ExportCompassCaveAction;
    QAction* ExportCompassRegionAction;

};

/**
    Gets the tree model that this survey export manager uses to update it's actions
  */
inline cwRegionTreeModel *cwSurveyExportManager::cavingRegionTreeModel() const {
    return NULL;
}




#endif // CWSURVEYEXPORTMANGER_H
