#ifndef CWSCRAPMANAGER_H
#define CWSCRAPMANAGER_H

//Qt includes
#include <QObject>
#include <QModelIndex>
#include <QSet>

//Our includes
class cwCavingRegion;
class cwCave;
class cwTrip;
class cwSurveyNoteModel;
class cwScrap;
class cwNote;

/**
    The scrap manager listens to changes in the notes and creates all
    the geometry need to show a scrap in 3d
  */
class cwScrapManager : public QObject
{
    Q_OBJECT
public:
    explicit cwScrapManager(QObject *parent = 0);
    
    void setRegion(cwCavingRegion* region);

signals:
    
public slots:

private:
    cwCavingRegion* Region;

    QSet<cwScrap*> Scraps;

    void connectAllCaves();
    void connectCave(cwCave* cave);
    void connectTrip(cwTrip* trip);
    void connectNoteModel(cwSurveyNoteModel* noteModel);
    void connectNote(cwNote* note);
    void connectScrapes(QList<cwScrap *> scraps);

    void updateScrapGeometry(QList<cwScrap *> scraps);

private slots:
    void cavesInserted(int begin, int end);
    void cavesRemoved(int begin, int end);
    void tripsInserted(int begin, int end);
    void tripsRemoved(int begin, int end);
    void notesInserted(QModelIndex parent, int begin, int end);
    void notesRemoved(QModelIndex parent, int begin, int end);
    void scrapInserted(int begin, int end);
    void scrapRemoved(int begin, int end);
    void updateScrapPoints();


};

#endif // CWSCRAPMANAGER_H
