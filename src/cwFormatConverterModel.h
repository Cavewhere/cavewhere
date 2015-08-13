/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWFORMATCONVERTERMODEL_H
#define CWFORMATCONVERTERMODEL_H

//Qt includes
#include <QObject>
#include <QAbstractListModel>
#include <QPointer>

//Our includes
class cwCavingRegion;
class cwProject;
class cwNote;

/**
 * @brief The cwFormatConverterModel class
 *
 * This model searches through all the notes in the cwCavingRegion and finds images that need
 * to be saved or found on disk. The user must either save the images to disk specify the location
 * of the images by adding path, using addSearchPath().
 */
class cwFormatConverterModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(cwCavingRegion* region READ region WRITE setRegion NOTIFY regionChanged)
    Q_PROPERTY(cwProject* project READ project WRITE setProject NOTIFY projectChanged)
    Q_PROPERTY(int numberOfResolvedImages READ numberOfResolvedImages NOTIFY numberOfResolvedImagesChanged)
    Q_PROPERTY(int numberOfImages READ numberOfImages NOTIFY numberOfImagesChanged)

    Q_ENUMS(Status Roles)
public:
    enum Status {
        NeedsResolution,
        FoundOnFileSystem,
        SavedToFileSystem
    };

    enum Roles {
        StatusRole,
        CaveNameRole,
        TripNameRole,
        ImageNumberRole,
        ImageRole,
        PathRole
    };

    cwFormatConverterModel(QObject* parent = nullptr);

    cwCavingRegion* region() const;
    void setRegion(cwCavingRegion* region);

    cwProject* project() const;
    void setProject(cwProject* project);

    Q_INVOKABLE void addSearchPath(QString path);
    Q_INVOKABLE void saveImages(QString directory);
    Q_INVOKABLE void queryDatabaseImages();
    Q_INVOKABLE void save();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;

    int numberOfResolvedImages() const;
    int numberOfImages() const;

signals:
    void regionChanged();
    void projectChanged();
    void numberOfImagesChanged();
    void numberOfResolvedImagesChanged();

private:
    class Row {
    public:
        QString CaveName;
        QString TripName;
        QPointer<cwNote> Note;
        QByteArray SHA1;
        QString Extension;
        QString OriginalPath; //Found by the user
        cwFormatConverterModel::Status CurrentStatus;
    };

    //Populated form the SearchPaths
    QMap<QByteArray, QString> ChecksumToPathLookup;

    QPointer<cwCavingRegion> Region; //!<
    QPointer<cwProject> Project; //!<

    QList<Row> Rows;

};

/**
* @brief cwFormatConverterModel::numberOfImages
* @return The number of images in the model, same as calling rowCount(), useful for QML
*/
inline int cwFormatConverterModel::numberOfImages() const {
    return rowCount();
}

#endif // CWFORMATCONVERTERMODEL_H
