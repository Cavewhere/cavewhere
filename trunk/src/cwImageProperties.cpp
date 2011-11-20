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
