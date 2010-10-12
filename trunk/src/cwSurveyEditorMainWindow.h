#ifndef SURVEYEDITORMAINWINDOW_H
#define SURVEYEDITORMAINWINDOW_H

//Our includes
#include "ui_cwSurveyEditorMainWindow.h"
class cwSurveyImporter;
class cwSurveyChunkGroup;

class cwSurveyEditorMainWindow : public QMainWindow, private Ui::cwSurveyEditorMainWindow
{
    Q_OBJECT

public:
    explicit cwSurveyEditorMainWindow(QWidget *parent = 0);

protected:
    cwSurveyImporter* SurvexImporter;
    cwSurveyChunkGroup* ChunkGroup;

    void changeEvent(QEvent *e);



protected slots:
    void ImportSurvex();
    void UpdateSurveyEditor();
    void ReloadQML();

};

#endif // SURVEYEDITORMAINWINDOW_H
