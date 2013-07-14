/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwWheelArea.h"

//Qt includes
#include <QGraphicsSceneWheelEvent>

cwWheelArea::cwWheelArea(QDeclarativeItem *parent) :
    QDeclarativeItem(parent)
{
}

/**
  Handles the wheel event
  */
void cwWheelArea::wheelEvent(QGraphicsSceneWheelEvent *event) {
    LastPosition = event->pos();
    if(event->orientation() == Qt::Vertical) {
        emit verticalScroll(event->delta());
    }
}
