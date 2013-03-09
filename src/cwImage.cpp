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
}

/**
 * @brief cwImage::scaleTexCoords
 * @param texCoords
 * @return This will scale the tex coords using clipArea.  This will fix texCoords to
 * map correctly to the image.
 */
QVector<QVector2D> cwImage::scaleTexCoords(QVector2D scaleTexCoords, QVector<QVector2D> texCoords)
{
    if(scaleTexCoords.x() == 1.0 && scaleTexCoords.y() == 1.0) {
        return texCoords;
    }

    for(int i = 0; i < texCoords.size(); i++) {
        QVector2D texCoord = texCoords.at(i);

        texCoords[i] = texCoord * scaleTexCoords;
    }

    return texCoords;
}
