/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwGlobals.h"

//Qt includes
#include <QFileInfo>
#include <QSettings>
#include <QApplication>
#include <QProcessEnvironment>
#include <QString>
#include <QDebug>
#include <QDirIterator>
#include <QFontDatabase>

//Std includes
#include "cwDebug.h"
#include "math.h"

/**
  If filename doesn't have an extension, the this function will try to add the
  extensionHint to the filename.

  filename - The filename that's going to be save
  extensionHint - the extension that the file should have. Example: "txt" or "zip"

  If the file already has an extension, this return the filename
  */
QString cwGlobals::addExtension(QString filename, QString extensionHint)
{
    if(QFileInfo(filename).completeSuffix().isEmpty())  {
        return filename + "." + extensionHint;
    }

    return filename;
}

/**
 * @brief cwGlobals::convertFromURL
 * @param fileUrl - The url that will be convert
 * @return Returns the converted url
 *
 * For example if fileUrl = file://SOME/LOCAL/FILENAME with will convert it to //SOME/LOCAL/FILENAME
 *
 * If the filenameUrl isn't a url, this just returns filenameUrl
 */
QString cwGlobals::convertFromURL(QString filenameUrl)
{
    QUrl fileUrl(filenameUrl);
    if(fileUrl.isValid() && fileUrl.isLocalFile()) {
        return fileUrl.toLocalFile();
    }
    return filenameUrl;
}

/**
 * @brief cwGlobals::findExecutable
 * @param executables - A list of executables, [cavern.exe, cavern]
 * @return Returns the first exectuable that exists base on the QApplication::applicationPath()
 */
QString cwGlobals::findExecutable(QStringList executables)
{
    auto paths = systemPaths();
    paths.append(QDir(QApplication::applicationDirPath()));
    return findExecutable(executables, paths);
}

/**
 * Returns the absolute file path to the first exectuable that exists from executables in dirs
 */
QString cwGlobals::findExecutable(const QStringList &executables, const QList<QDir> &dirs)
{
    for(const QString& appName : executables) {
        for(const auto& directory : dirs) {
            QString currentPath = directory.absoluteFilePath(appName);
            QFileInfo fileInfo(currentPath);
            if(fileInfo.exists() && fileInfo.isExecutable()) {
                return currentPath;
            }
        }
    }

    return QString();
}

/**
 * Returns all the directories in the PATH enviromental variable
 */
QList<QDir> cwGlobals::systemPaths()
{
    //This is static because the system enviroment shouldn't change and this is a slow
    //function based Qt documentation
    static const auto env = QProcessEnvironment::systemEnvironment();

    QString seperator;
#ifdef Q_OS_WIN
    seperator = ";";
#elif defined(Q_OS_UNIX)
    seperator = ":";
#endif

    QList<QDir> dirs;
    if(env.contains("PATH") && !seperator.isEmpty()) {
        QString path = env.value("PATH");
        QStringList dirStringList = path.split(seperator);
        for(auto dirStr : dirStringList) {
            QDir dir(dirStr);
            if(dir.exists()) {
                dirs.append(dir);
            }
        }
    }

    return dirs;
}

/**
 * Returns the path to default system's survex path aka the path to the bin directory
 * that cavern and survexport lives
 */
QList<QDir> cwGlobals::survexPath()
{
    QDir cavewhereSurvex = QCoreApplication::applicationDirPath() + "/survex";

#ifdef Q_OS_WIN
    return {cavewhereSurvex,
            QDir(QCoreApplication::applicationDirPath()),
            QDir(QStringLiteral("C:/Program Files (x86)/Survex")),
            QDir(QStringLiteral("C:/Program Files/Survex"))};
#elif defined(Q_OS_UNIX)
    QList<QDir> dirs;
    dirs.append(cavewhereSurvex);
    dirs.append(QDir(QCoreApplication::applicationDirPath()));
    dirs.append(QDir(QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources")));
    dirs.append(systemPaths());
    return dirs;
#else
    return {};
#endif
}

static bool isPathInsideDir(const QString& candidate, const QDir& baseDir) {
    if (candidate.isEmpty() || !baseDir.exists()) {
        return false;
    }

    const QString basePath = QDir::cleanPath(baseDir.absolutePath()) + QLatin1Char('/');
    const QString candidatePath = QDir::cleanPath(QFileInfo(candidate).absoluteFilePath());

#ifdef Q_OS_WIN
    const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif

    return candidatePath.compare(basePath.left(basePath.size() - 1), sensitivity) == 0
        || candidatePath.startsWith(basePath, sensitivity);
}

bool cwGlobals::isInApplicationDir(const QString& path)
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QDir resourcesDir(QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources"));
    return isPathInsideDir(path, appDir) || isPathInsideDir(path, resourcesDir);
}

/**
 * Init's cavewhere lib resources when built as a static library
 */
void cwGlobals::initilizeResources()
{
    Q_INIT_RESOURCE(cwResources);
    Q_INIT_RESOURCE(cwFonts);
}

void cwGlobals::loadFonts()
{
    // Specify the resource path where your fonts are located
    const QString fontResourcePath(":/fonts");

    // Iterate over all files in the resource directory (and subdirectories)
    QDirIterator it(fontResourcePath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString fontPath = it.next();
        int fontId = QFontDatabase::addApplicationFont(fontPath);
        // qDebug() << "FontFamily:" << QFontDatabase::applicationFontFamilies(fontId);
        if(fontId == -1) {
            qWarning() << "Failed to load font:" << fontPath << LOCATION;
        }
    }
}
