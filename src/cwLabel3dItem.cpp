/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLabel3dItem.h"

cwLabel3dItem::cwLabel3dItem()
{
}

cwLabel3dItem::cwLabel3dItem(QString text, QVector3D position, QFont font) :
    Font(font),
    Text(text),
    Position(position)
{
}


