/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwImageProperties.h"

cwImageProperties::cwImageProperties(QObject *parent) :
    QObject(parent)
{
}

/**
  Sets the properties for the image
  */
void cwImageProperties::setImage(cwImage image) {
    if(Image != image) {
        Image = image;
        emit sizeChanged();
        emit dotsPerMeterChanged();
    }
}
