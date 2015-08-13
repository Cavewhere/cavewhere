/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwFormatConverterModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwImageProvider.h"
#include "cwProject.h"
#include "cwRemoveImageTask.h"

//Qt includes
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>

cwFormatConverterModel::cwFormatConverterModel(QObject *parent) :
    QAbstractListModel(parent)
{

}

/**
* @brief cwFormatConverterModel::setRegion
* @param region
*/
void cwFormatConverterModel::setRegion(cwCavingRegion* region) {
    if(Region != region) {
        Region = region;
        emit regionChanged();
    }
}

/**
 * @brief cwFormatConverterModel::queryDatabaseImages
 *
 * This goes through all the images in the database and sha1 each image. The SHA1 is used as a
 * checksum to compare images located on disk.
 */
void cwFormatConverterModel::queryDatabaseImages()
{
    Q_ASSERT(!Project.isNull());
    Q_ASSERT(!Region.isNull());

    cwImageProvider imageProvider;
    imageProvider.setProjectPath(project()->filename());

    beginResetModel();

    foreach(cwCave* cave, Region->caves()) {
        foreach(cwTrip* trip, cave->trips()) {
            foreach(cwNote* note, trip->notes()->notes()) {
                cwImage image = note->image();
                if(image.original() > 0) {
                    cwImageData imageData = imageProvider.data(image.original());
                    QByteArray sha1 = QCryptographicHash::hash(imageData.data(), QCryptographicHash::Sha1);

                    Row row;
                    row.CaveName = cave->name();
                    row.TripName = trip->name();
                    row.Note = note;
                    row.SHA1 = sha1;
                    row.CurrentStatus = NeedsResolution;
                    row.Extension = QString::fromLocal8Bit(imageData.format());

                    Rows.append(row);
                }
            }
        }
    }

    emit numberOfImagesChanged();
    endResetModel();
}

/**
 * @brief cwFormatConverterModel::save
 *
 * This goes through and updates all the cwNote, saves the images to disk. And moves the SQLite cw
 * file to cwCache file, and then saves the cw file.
 */
void cwFormatConverterModel::save()
{
    Q_ASSERT(numberOfResolvedImages() == numberOfImages());

    //Copy the project file to the cache
    QString newFilename = project()->cacheFilename();
    QString oldFilename = project()->filename();
    QFile::copy(oldFilename, newFilename);

    cwImageProvider imageProvider;
    imageProvider.setProjectPath(newFilename);

    QList<cwImage> imagesToDelete;

    //Copy origianl images to disk
    foreach(const Row& row, Rows) {
        Q_ASSERT(row.CurrentStatus != NeedsResolution);

        int originalId = row.Note->image().original();

        if(row.CurrentStatus == SavedToFileSystem) {
            cwImageData imageData = imageProvider.data(originalId);
            QFile file(row.OriginalPath);
            file.open(QFile::WriteOnly);
            file.write(imageData.data());
            file.close();
        }

        //Update Notes with the new paths
        cwImage noteImage = row.Note->image();
        noteImage.setOriginal(-1);
        noteImage.setOriginalFilename(row.OriginalPath);
        noteImage.setOriginalChecksum(row.SHA1);
        row.Note->image();

        //Delete original images
        cwImage imageToDelete;
        imageToDelete.setOriginal(originalId);
        imagesToDelete.append(imageToDelete);
    }

    cwRemoveImageTask removeImages;
    removeImages.setDatabaseFilename(newFilename);
    removeImages.setImagesToRemove(imagesToDelete);
    removeImages.start();

    //Call save on the the project
    project()->save();
}

/**
* @brief cwFormatConverterModel::setProject
* @param project
*/
void cwFormatConverterModel::setProject(cwProject* project) {
    if(Project != project) {
        Project = project;
        emit projectChanged();
    }
}

/**
 * @brief cwFormatConverterModel::addSearchPath
 * @param path
 */
void cwFormatConverterModel::addSearchPath(QString path)
{
    path = cwProject::convertFromURL(path);

    QStringList filters;
    filters << "*.jp2" << "*.jpg" << "*.jpeg" << "*.tif" << "*.png";
    QDirIterator dirIterator(path, filters, QDir::NoFilter, QDirIterator::Subdirectories);
    while(dirIterator.hasNext()) {

        dirIterator.next();
        QString filePath = dirIterator.filePath();
        QFile file(filePath);
        file.open(QFile::ReadOnly);
        QByteArray imageData = file.readAll();
        QByteArray sha1 = QCryptographicHash::hash(imageData, QCryptographicHash::Sha1);
        file.close();

        ChecksumToPathLookup.insert(sha1, filePath);
    }

    //Update the status of rows that were found
    QVector<int> rolesChanged;
    rolesChanged << PathRole << StatusRole;

    for(int i = 0; i < Rows.size(); i++) {
        const Row& row = Rows.at(i);
        if(row.CurrentStatus == NeedsResolution) {
            if(ChecksumToPathLookup.contains(row.SHA1)) {
                //Found a matching image
                QString path = ChecksumToPathLookup.value(row.SHA1);
                Rows[i].OriginalPath = path;
                Rows[i].CurrentStatus = FoundOnFileSystem;

                QModelIndex rowIndex = index(i);

                emit dataChanged(rowIndex, rowIndex, rolesChanged);
            }
        }
    }

    emit numberOfResolvedImagesChanged();
}

/**
 * @brief cwFormatConverterModel::saveImages
 */
void cwFormatConverterModel::saveImages(QString directory)
{
    directory = cwProject::convertFromURL(directory);

    //Update the status of rows that were found
    QVector<int> rolesChanged;
    rolesChanged << PathRole << StatusRole;

    for(int i = 0; i < Rows.size(); i++) {
        Row& row = Rows[i];
        if(row.CurrentStatus == NeedsResolution) {
            //Create a path for the row
            QModelIndex rowIndex = index(i);
            int numberOfNotes = row.Note->parentTrip()->notes()->notes().size();
            int noteIndex = data(rowIndex, ImageNumberRole).toInt();

            QString pathName = QString("%1/%2 %3 %4of%5.%6")
                    .arg(directory)
                    .arg(row.CaveName)
                    .arg(row.TripName)
                    .arg(noteIndex)
                    .arg(numberOfNotes)
                    .arg(row.Extension);

            row.OriginalPath = pathName;
            row.CurrentStatus = SavedToFileSystem;

            emit dataChanged(rowIndex, rowIndex, rolesChanged);
        }
    }

    emit numberOfResolvedImagesChanged();
}

/**
 * @brief cwFormatConverterModel::rowCount
 * @param parent
 * @return Number of images that need to be converted.
 */
int cwFormatConverterModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return Rows.size();
}

/**
 * @brief cwFormatConverterModel::data
 * @param index
 * @param role
 * @return
 */
QVariant cwFormatConverterModel::data(const QModelIndex &index, int role) const
{
    const Row& row = Rows.at(index.row());

    switch(role) {
    case StatusRole:
        return row.CurrentStatus;
    case CaveNameRole:
        return row.CaveName;
    case TripNameRole:
        return row.TripName;
    case ImageNumberRole:
        return row.Note->parentTrip()->notes()->notes().indexOf(row.Note) + 1;
    case ImageRole:
        return row.Note->image().original();
    case PathRole:
        return row.OriginalPath;
    }

    return QVariant();
}

/**
 * @brief cwFormatConverterModel::roleNames
 * @return
 */
QHash<int, QByteArray> cwFormatConverterModel::roleNames() const
{
    QHash<int, QByteArray> roles;

    roles.insert(StatusRole, "statusRole");
    roles.insert(CaveNameRole, "caveNameRole");
    roles.insert(TripNameRole, "tripNameRole");
    roles.insert(ImageNumberRole, "imageNumberRole");
    roles.insert(ImageRole, "imageRole");
    roles.insert(PathRole, "pathRole");

    return roles;
}

/**
* @brief cwFormatConverterModel::region
* @return
*/
cwCavingRegion* cwFormatConverterModel::region() const {
    return Region;
}

/**
* @brief cwFormatConverterModel::project
* @return
*/
cwProject* cwFormatConverterModel::project() const {
    return Project;
}

/**
* @brief cwFormatConverterModel::numberOfImagesToResolve
* @return Number of images that still need to be resolved
*/
int cwFormatConverterModel::numberOfResolvedImages() const {
    int count = 0;
    foreach(const Row& row, Rows) {
        if(row.CurrentStatus != NeedsResolution) {
            count++;
        }
    }
    return count;
}
