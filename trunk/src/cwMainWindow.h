#ifndef SURVEYEDITORMAINWINDOW_H
#define SURVEYEDITORMAINWINDOW_H


//Our includes
#include "ui_cwMainWindow.h"
class cwSurvexImporter;
class cwSurvexExporter;
class cwTrip;
class cwCave;
class cwSurveyNoteModel;
class cwImportSurvexDialog;
class cwCavingRegion;
class cwRegionTreeModel;
class cwLinePlotManager;
class cwProject;
class cwProjectImageProvider;

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

    //For undo / redo
    void updateUndoText(QString undoText);
    void updateRedoText(QString redoText);

    //For saving/loading the project
    void save();
    void load();

private:
    cwSurvexExporter* SurvexExporter;

    //Survey data
    cwCavingRegion* Region;
    cwRegionTreeModel* RegionTreeModel;
    //QTreeView* RegionTreeView;

    QThread* ExportThread;

    //Loop closer manager
    cwLinePlotManager* LinePlotManager;

    //For undo and redo
    QAction* UndoAction;
    QAction* RedoAction;

    //Undo / redo
    QUndoStack* UndoStack;

    //Save / Load
    cwProject* Project;

    QGLWidget* createGLWidget();
    void initGLEW();

    void initialWindowShape();
//    cwCave* currentSelectedCave() const;
//    cwTrip* currentSelectedTrip() const;
};

#endif // SURVEYEDITORMAINWINDOW_H
