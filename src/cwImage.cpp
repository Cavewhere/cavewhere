//Our includes
#include "cwImage.h"
#include "cwGunZipReader.h"
#include "cwProject.h"

//Qt includes
#include <QDir>
#include <QDebug>

cwImage::cwImage() :
    Data(new PrivateData())
{

}

cwImage::PrivateData::PrivateData() {
    IconId = -1;
    OriginalId = -1;
    OriginalDotsPerMeter = 0;
}
