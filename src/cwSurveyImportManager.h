#ifndef CWSURVEYIMPORTMANAGER_H
#define CWSURVEYIMPORTMANAGER_H

//Qt includes
#include <QObject>
#include <QUndoStack>

//Our includes
class cwCavingRegion;

/**
    This class allows qml to import data
  */
class cwSurveyImportManager : public QObject
{
    Q_OBJECT
public:
    explicit cwSurveyImportManager(QObject *parent = 0);
    
    void setCavingRegion(cwCavingRegion* region);
    cwCavingRegion* cavingRegion() const;

    void setUndoStack(QUndoStack* undoStack);

    Q_INVOKABLE void importSurvex();

signals:
    
public slots:

private:
    cwCavingRegion* CavingRegion;
    QUndoStack* UndoStack;


};

inline cwCavingRegion *cwSurveyImportManager::cavingRegion() const
{
    return CavingRegion;
}

inline void cwSurveyImportManager::setUndoStack(QUndoStack *undoStack)
{
    UndoStack = undoStack;
}



#endif // CWSURVEYIMPORTMANAGER_H
