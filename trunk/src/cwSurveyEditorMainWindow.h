#ifndef SURVEYEDITORMAINWINDOW_H
#define SURVEYEDITORMAINWINDOW_H

//Our includes
#include "ui_cwSurveyEditorMainWindow.h"
class cwSurveyImporter;
class cwSurvexExporter;
class cwSurveyTrip;
class cwSurveyNoteModel;

class cwSurveyEditorMainWindow : public QMainWindow, private Ui::cwSurveyEditorMainWindow
{
    Q_OBJECT

public:
    explicit cwSurveyEditorMainWindow(QWidget *parent = 0);

protected:
    cwSurveyImporter* SurvexImporter;
    cwSurvexExporter* SurvexExporter;
    cwSurveyTrip* ChunkGroup;
    cwSurveyNoteModel* NoteModel;

    void changeEvent(QEvent *e);



protected slots:
    void ExportSurvex();
    void ImportSurvex();
    void UpdateSurveyEditor();
    void ReloadQML();

};

#endif // SURVEYEDITORMAINWINDOW_H
