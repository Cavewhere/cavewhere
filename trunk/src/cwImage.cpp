//Our includes
#include "cwImage.h"
#include "cwGunZipReader.h"
#include "cwProject.h"

//Qt includes
#include <QDir>
#include <QDebug>

cwImage::cwImage()
{
    IconId = -1;
    OriginalId = -1;
}

///**
//  This will get the data for a mipmap level.  If the level doesn't exist
//  or then a empty QByteArray is returned.  This will also decompress the
//  mipmap from zLib.  The mipmap will be returned as dxt1 compression.
//  */
//QByteArray cwImage::mipmap(cwProject* project, int level) {

//    //Make sure the mipmap level is good
//    if(level < 0 || level >= Mipmaps.size()) {
//        qDebug() << "Mipmap level doesn't exist:" << level;
//        return QByteArray();
//    }

//    QString mipmapFilename = project->projectPath().absoluteFilePath(Mipmaps[level]);

//    //Read the data
//    cwGunZipReader reader;
//    reader.setFilename(mipmapFilename);
//    reader.start();

//    if(reader.hasErrors()) {
//        qDebug() << "Can't extract mipmap:" << reader.errors().join("\n");
//        return QByteArray();
//    }

//    return reader.data();
//}

///**
//  Get's the icon.

//  If the icon is invalid the a empty QImage is returned.
//  */
//QImage cwImage::icon(cwProject* project) const {
//    QString absoluteFilePath = project->projectPath().absoluteFilePath(IconFilename);
//    return QImage(absoluteFilePath);
//}

///**
//  Gets the original file
//  */
//QImage cwImage::original(cwProject* project) {
//    QString absoluteFilePath = project->projectPath().absoluteFilePath(OriginalFilename);
//    return QImage(absoluteFilePath);
//}
