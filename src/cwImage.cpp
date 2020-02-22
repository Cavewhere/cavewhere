/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

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

bool cwImage::isMipmapsValid() const
{
    if(Data->Mipmaps.isEmpty()) {
        return false;
    }
    auto iter = std::find_if(Data->Mipmaps.begin(), Data->Mipmaps.end(),
                             [](int id) { return !cwImage::isIdValid(id); });
    return iter == Data->Mipmaps.end();
}



cwImage::PrivateData::PrivateData() {
    IconId = -1;
    OriginalId = -1;
    OriginalDotsPerMeter = 0;
}

