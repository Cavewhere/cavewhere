/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef LOADPROJECTHELPER
#define LOADPROJECTHELPER

//Qt includes
#include <QVector3D>
#include <QString>
#include <QObject>
#include <QTemporaryDir>

//Our includes
#include "cwProject.h"
#include "CaveWhereTestLibExport.h"

//Std includes
#include <iostream>

/**
 * Copyies filename to the temp folder
 */
QString CAVEWHERE_TESTLIB_EXPORT copyToTempFolder(QString filename);

QString CAVEWHERE_TESTLIB_EXPORT prependTempFolder(QString filename);

/**
 * @brief fileToProject
 * @param filename
 * @return A new project generate from filename
 */
std::shared_ptr<cwProject> CAVEWHERE_TESTLIB_EXPORT fileToProject(QString filename);
QString CAVEWHERE_TESTLIB_EXPORT fileToProject(cwProject* project, const QString& filename);

class CAVEWHERE_TESTLIB_EXPORT TestHelper : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    Q_INVOKABLE void loadProjectFromFile(cwProject* project, const QString& filename) {
        fileToProject(project, filename);
    }

    Q_INVOKABLE void loadProjectFromZip(cwProject* project, const QString& filename);

    Q_INVOKABLE QString copyToTempDir(const QString& filename);

    Q_INVOKABLE bool fileExists(const QUrl& filename) const;
    Q_INVOKABLE size_t fileSize(const QUrl& filename) const;
    Q_INVOKABLE void removeFile(const QUrl& filename) const;

    Q_INVOKABLE QUrl tempDirectoryUrl() {
        QTemporaryDir dir;
        dir.setAutoRemove(false);
        return QUrl::fromLocalFile(dir.path());
    }

    Q_INVOKABLE QUrl toLocalUrl(const QString& path) {
        return QUrl::fromLocalFile(path);
    }

};

#endif // LOADPROJECTHELPER

