#ifndef SURVEYEDITORMAINWINDOW_H
#define SURVEYEDITORMAINWINDOW_H

//Our includes
#include "ui_cwSurveyEditorMainWindow.h"
class cwSurvexImporter;
class cwSurvexExporter;
class cwTrip;
class cwSurveyNoteModel;
class cwImportSurvexDialog;
class cwCavingRegion;
class cwRegionTreeModel;

//Qt includes
#include <QString>
class QTreeView;

class cwSurveyEditorMainWindow : public QMainWindow, private Ui::cwSurveyEditorMainWindow
{
    Q_OBJECT

public:
    explicit cwSurveyEditorMainWindow(QWidget *parent = 0);

protected:
    cwSurvexExporter* SurvexExporter;
    cwTrip* Trip;
    cwSurveyNoteModel* NoteModel;

    //Survey data
    cwCavingRegion* Region;
    cwRegionTreeModel* RegionTreeModel;
    QTreeView* RegionTreeView;

    void changeEvent(QEvent *e);


protected slots:
    void ExportSurvex();
    void ImportSurvex();
    void UpdateSurveyEditor();
    void ReloadQML();

    //For changing
    void SetSurveyData(QItemSelection selected, QItemSelection deselected);

};

#endif // SURVEYEDITORMAINWINDOW_H
