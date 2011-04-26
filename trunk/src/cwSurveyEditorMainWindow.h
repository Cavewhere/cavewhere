#ifndef SURVEYEDITORMAINWINDOW_H
#define SURVEYEDITORMAINWINDOW_H

//Our includes
#include "ui_cwSurveyEditorMainWindow.h"
class cwSurvexImporter;
class cwSurvexExporter;
class cwTrip;
class cwCave;
class cwSurveyNoteModel;
class cwImportSurvexDialog;
class cwCavingRegion;
class cwRegionTreeModel;
class cwLinePlotManager;

//Qt includes
#include <QString>
class QTreeView;

class cwSurveyEditorMainWindow : public QMainWindow, private Ui::cwSurveyEditorMainWindow
{
    Q_OBJECT

public:
    explicit cwSurveyEditorMainWindow(QWidget *parent = 0);



    void changeEvent(QEvent *e);

protected slots:
    void openExportSurvexTripFileDialog();
    void exportSurvexTrip(QString filename);
    void exporterFinished();

    void openExportSurvexCaveFileDialog();
    void exportSurvexCave(QString filename);

    void openExportSurvexRegionFileDialog();
    void exportSurvexRegion(QString filename);

    void openExportCompassCaveFileDialog();
    void exportCaveToCompass(QString filename);

    void importSurvex();
    void reloadQML();

    //For changing
    void setSurveyData(QItemSelection selected, QItemSelection deselected);

private:
    cwSurvexExporter* SurvexExporter;
    cwSurveyNoteModel* NoteModel;

    //Survey data
    cwCavingRegion* Region;
    cwRegionTreeModel* RegionTreeModel;
    //QTreeView* RegionTreeView;

    QThread* ExportThread;

    //Loop closer manager
    cwLinePlotManager* LinePlotManager;

//    cwCave* currentSelectedCave() const;
//    cwTrip* currentSelectedTrip() const;
};

#endif // SURVEYEDITORMAINWINDOW_H
