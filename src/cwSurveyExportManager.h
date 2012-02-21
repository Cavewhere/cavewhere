#ifndef CWSURVEYEXPORTMANGER_H
#define CWSURVEYEXPORTMANGER_H

//Qt includes
#include <QObject>
#include <QModelIndex>
class QAction;
class QMenu;
class QItemSelectionModel;
class QThread;

//Our includes
class cwRegionTreeModel;
class cwCave;
class cwTrip;

/**
    This class will open dialogs to export data.

    This class controls the different survey data exporters: survex and compass
  */
class cwSurveyExportManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString currentCaveName READ currentCaveName NOTIFY updateMenu)
    Q_PROPERTY(QString currentTripName READ currentTripName NOTIFY updateMenu)

public:
    explicit cwSurveyExportManager(QObject *parent = 0);
    ~cwSurveyExportManager();

    QItemSelectionModel* regionSelectionModel() const;
    void setRegionSelectionModel(QItemSelectionModel* selectionModel);

    cwRegionTreeModel* cavingRegionTreeModel() const;
    void setCavingRegionTreeModel(cwRegionTreeModel* model);

    QString currentCaveName() const;
    QString currentTripName() const;

signals:
    void updateMenu();

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

    void updateActions();

private:
    //This model allows use to export different data
    cwRegionTreeModel* Model;
    QItemSelectionModel* SelectionModel; //Query which item is selected

    //The thread that the exporting runs on
    QThread* ExportThread;

    void updateCaveActions(const QModelIndex& index);
    void updateTripActions(const QModelIndex& index);

    cwCave* currentCave() const;
    cwTrip* currentTrip() const;

};

/**
    Gets the tree model that this survey export manager uses to update it's actions
  */
inline cwRegionTreeModel *cwSurveyExportManager::cavingRegionTreeModel() const {
    return NULL;
}






#endif // CWSURVEYEXPORTMANGER_H
