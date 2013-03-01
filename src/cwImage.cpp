//Our includes
#include "cwImage.h"
#include "cwGunZipReader.h"
#include "cwProject.h"

//Qt includes
#include <QDir>
#include <QDebug>
#include <QVector2D>
#include <QVector>

cwImage::cwImage() :
    Data(new PrivateData())
{

}

cwImage::PrivateData::PrivateData() {
    IconId = -1;
    OriginalId = -1;
    OriginalDotsPerMeter = 0;
    ClipArea = QSizeF(1.0, 1.0);
}

/**
 * @brief cwImage::scaleTexCoords
 * @param texCoords
 * @return This will scale the tex coords using clipArea.  This will fix texCoords to
 * map correctly to the image.
 */
QVector<QVector2D> cwImage::scaleTexCoords(QVector<QVector2D> texCoords) const
{
    double width = clipArea().width();
    double height = clipArea().height();

    if(width == 1.0 && height == 1.0) {
        return texCoords;
    }

    for(int i = 0; i < texCoords.size(); i++) {
        QVector2D texCoord = texCoords.at(i);

        texCoord.setX(texCoord.x() * width);
        texCoord.setY(texCoord.y() * height);

        texCoords[i] = texCoord;
    }

    return texCoords;
}
