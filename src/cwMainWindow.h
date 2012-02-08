#ifndef SURVEYEDITORMAINWINDOW_H
#define SURVEYEDITORMAINWINDOW_H


//Our includes
#include "ui_cwMainWindow.h"
class cwSurvexImporter;
class cwSurvexExporter;
class cwImportSurvexDialog;
class cwRootData;

//Qt includes
#include <QString>
#include <QUndoStack>
class QTreeView;
class QGLWidget;

class cwMainWindow : public QMainWindow, private Ui::cwMainWindow
{
    Q_OBJECT

public:
    explicit cwMainWindow(QWidget *parent = 0);

    void changeEvent(QEvent *e);

protected slots:

    void importSurvex();
    void reloadQML();

    //For changing
    void setSurveyData(QItemSelection selected, QItemSelection deselected);

    //For undo / redo
    void updateUndoText(QString undoText);
    void updateRedoText(QString redoText);

    //For saving/loading the project
    void save();
    void load();

private:
    cwSurvexExporter* SurvexExporter;

    //The global objects that all qml objects have access to
    cwRootData* Data;

    QThread* ExportThread;

    //For undo and redo
    QAction* UndoAction;
    QAction* RedoAction;

    //Undo / redo
    QUndoStack* UndoStack;

    QGLWidget* createGLWidget();
    void initGLEW();

    void initialWindowShape();
//    cwCave* currentSelectedCave() const;
//    cwTrip* currentSelectedTrip() const;
};

#endif // SURVEYEDITORMAINWINDOW_H
