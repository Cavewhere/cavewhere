/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwGLMapperItem.h"

cwGLMapperItem::cwGLMapperItem(QDeclarativeItem *parent) :
    QDeclarativeItem(parent)
{

}

/**
  This set the position in the model coordinates system.
  */
void cwGLMapperItem::setPosition(QVector3D modelPosition) {
    if(Position != modelPosition) {
        Position = modelPosition;
        emit positionChanged();
    }
}
