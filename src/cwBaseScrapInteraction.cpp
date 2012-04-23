//Our includes
#include "cwBaseScrapInteraction.h"
#include "cwDebug.h"
#include "cwScrap.h"
#include "cwNote.h"

cwBaseScrapInteraction::cwBaseScrapInteraction(QQuickItem *parent) :
    cwNoteInteraction(parent),
    CurrentScrapIndex(-1)
{
    connect(this, SIGNAL(noteChanged()), SLOT(startNewScrap()));
}

/**
    This adds a new scrap to the note and also make the current scrap equal to the newly add
    scrap
  */
void cwBaseScrapInteraction::addScrap() {
    cwScrap* scrap = new cwScrap();
    note()->addScrap(scrap);
    CurrentScrapIndex = note()->scraps().size() - 1;
}

/**
    Add a point to the end of the current scrap.

    If no scrap is currently selected, this will create a new scrap
  */
void cwBaseScrapInteraction::addPoint(QPointF noteCoordinate) {
    if(note() == NULL) {
        qDebug() << "This is a bug! Can't add point because Note is null" << LOCATION;
        return;
    }

    if(CurrentScrapIndex == -1) {
        addScrap();
    }

    cwScrap* scrap = note()->scrap(CurrentScrapIndex);

    if(scrap == NULL) {
        qDebug() << "This is a bug! scrap is null" << LOCATION;
        return;
    }

    scrap->addPoint(noteCoordinate);
}

/**
  This starts a new scrap instead of
  */
 void cwBaseScrapInteraction::startNewScrap() {
    CurrentScrapIndex = -1;
 }
