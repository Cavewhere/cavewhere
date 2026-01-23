#include "cwSaveLoad.h"
#include "cwDebug.h"
#include "cwTrip.h"
#include "cwRegionSaveTask.h"
#include "cwRegionLoadTask.h"
#include "cwConcurrent.h"
#include "cwFutureManagerModel.h"
#include "cwTeam.h"
#include "cwCavingRegion.h"
#include "cwProject.h"
#include "cwSurveyNoteModel.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwNote.h"
#include "cwImageProvider.h"
#include "cavewhereVersion.h"
#include "cwPlanScrapViewMatrix.h"
#include "cwRunningProfileScrapViewMatrix.h"
#include "cwRegionTreeModel.h"
#include "cwScrap.h"
#include "cwPDFConverter.h"
#include "cwUnits.h"
#include "cwPDFSettings.h"
#include "cwImageUtils.h"
#include "cwSvgReader.h"
#include "cwNoteLiDAR.h"
#include "cwImageResolution.h"
#include "cwUniqueConnectionChecker.h"
#include "cwGlobals.h"
#include "cwGitIgnore.h"

//Async future
#include <asyncfuture.h>

//Google protobuffer
#include <google/protobuf/util/json_util.h>
#include "google/protobuf/message.h"
#include "cavewhere.pb.h"
#include "qt.pb.h"

//Qt includes
#include <QtConcurrent>
#include <QSaveFile>
#include <QSet>
#include <QImageReader>
#include <QFile>

#ifdef CW_WITH_PDF_SUPPORT
#include <QPdfDocument>
#endif
#include <QImage>
#include <QUuid>
#include <QtCore/qscopeguard.h>
#include <QFileInfo>

//QQuickGit
#include "GitRepository.h"

//Monad
#include "Monad/Monad.h"

//std includes
#include <unordered_set>
#include <algorithm>

using namespace Monad;

static QStringList fromProtoStringList(const google::protobuf::RepeatedPtrField<std::string> &protoStringList);

// template<typename NoteT>
// static QString noteRelativePathHelper(const NoteT* note, const QString& storedPath);

namespace {

cwImage::OriginalImageInfo imageInfo(const QString& path) {
    cwImage::OriginalImageInfo info;
    const QFileInfo fileInfo(path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return info;
    }

    QByteArray imageData = file.readAll();
    if (imageData.isEmpty()) {
        return info;
    }

    const QByteArray format = fileInfo.suffix().toLatin1();
    if (fileInfo.suffix().compare(QStringLiteral("svg"), Qt::CaseInsensitive) == 0) {
        return cwSvgReader::imageInfo(imageData, format);
    } else {
        QImage image = cwImageUtils::imageWithAutoTransform(imageData, format);
        if (image.isNull()) {
            return info;
        }

        info.originalSize = image.size();
        info.originalDotsPerMeter = image.dotsPerMeterX();
        info.unit = cwImage::Unit::Pixels;
    }

    return info;
}

#ifdef CW_WITH_PDF_SUPPORT
cwImage::OriginalImageInfo imageInfo(const QPdfDocument& document, int pageIndex) {
    cwImage::OriginalImageInfo info;
    if (pageIndex < 0 || pageIndex >= document.pageCount()) {
        return info;
    }

    const QSizeF pagePoints = document.pagePointSize(pageIndex);
    const QSize pointSize(qRound(pagePoints.width()),
                          qRound(pagePoints.height()));
    constexpr double inchesToMeters = cwUnits::convert(1.0, cwUnits::Inches, cwUnits::Meters);
    constexpr double pointsToInches = 1.0 / cwUnits::PointsPerInch;
    constexpr int pointsPerMeter = qRound(1.0 / (pointsToInches * inchesToMeters));

    info.originalSize = pointSize;
    info.originalDotsPerMeter = pointsPerMeter;
    info.unit = cwImage::Unit::Points;

    return info;
}
#endif

cwImage loadImage(const CavewhereProto::Image& protoImage, const QString& noteFilename) {
    cwImage image;

    const QString rawImagePath = QString::fromStdString(protoImage.path());
    const QString imageFileName = QFileInfo(rawImagePath).fileName();
    image.setPath(imageFileName.isEmpty() ? rawImagePath : imageFileName);

    if (protoImage.has_page()) {
        image.setPage(protoImage.page());
    }

    const bool hasImageUnit = protoImage.has_imageunit();
    if (hasImageUnit) {
        image.setUnit(static_cast<cwImage::Unit>(protoImage.imageunit()));
    }

    const bool hasSize = protoImage.has_size();
    if (hasSize) {
        image.setOriginalSize(cwRegionLoadTask::loadSize(protoImage.size()));
    }

    const bool hasDots = protoImage.has_dotpermeter();
    if (hasDots) {
        image.setOriginalDotsPerMeter(protoImage.dotpermeter());
    }

    const QString suffix = QFileInfo(imageFileName).suffix().toLower();
    const bool isPdf = suffix == QStringLiteral("pdf");
    const bool isSvg = suffix == QStringLiteral("svg");

    if (!hasImageUnit) {
        if (isPdf) {
            image.setUnit(cwImage::Unit::Points);
        } else if (isSvg) {
            image.setUnit(cwImage::Unit::SvgUnits);
        } else {
            image.setUnit(cwImage::Unit::Pixels);
        }
    }

    const bool sizeMissing = !hasSize || !image.originalSize().isValid();
    const bool dotsMissing = !hasDots || image.originalDotsPerMeter() <= 0;
    const bool unitMismatch = (isPdf && image.unit() != cwImage::Unit::Points)
        || (isSvg && image.unit() != cwImage::Unit::SvgUnits)
        || (!isPdf && !isSvg && image.unit() != cwImage::Unit::Pixels);
    const bool needsReload = sizeMissing
                             || dotsMissing
                             || (!hasImageUnit && image.unit() != cwImage::Unit::Pixels)
                             || unitMismatch;

    if (needsReload) {
        qDebug() << "Image needs reload:" << noteFilename << sizeMissing << dotsMissing << !hasImageUnit << unitMismatch;

        const QDir noteDir = QFileInfo(noteFilename).dir();
        const QString imagePath = noteDir.absoluteFilePath(image.path());
        const QFileInfo imageFileInfo(imagePath);
        if (imageFileInfo.exists()) {
            if (isPdf) {
#ifdef CW_WITH_PDF_SUPPORT
                QPdfDocument document;
                if (document.load(imagePath) == QPdfDocument::Error::None) {
                    const int pageIndex = image.page() >= 0 ? image.page() : 0;
                    const cwImage::OriginalImageInfo info = imageInfo(document, pageIndex);
                    if (info.originalSize.isValid()) {
                        image.setOriginalImageInfo(info);
                    }
                }
#endif
            } else {
                const cwImage::OriginalImageInfo info = imageInfo(imagePath);
                if (info.originalSize.isValid()) {
                    image.setOriginalImageInfo(info);
                }
            }
        }
    }

    return image;
}

} // namespace


template<typename ProtoType>
static Result<ProtoType> loadMessage(const QString& filename) {
    QFile file(filename);
    bool success = file.open(QFile::ReadOnly);
    if(!success) {
        return Result<ProtoType>(file.errorString());
    }

    auto allData = file.readAll();

    ProtoType proto;
    auto status = google::protobuf::util::JsonStringToMessage(allData.toStdString(), &proto);
    if (!status.ok()) {
        return Result<ProtoType>(QString("Failed to parse JSON: %1").arg(QString::fromStdString(status.message().data())));
    }
    return Result<ProtoType>(proto);
}

// template<typename NoteT>
// static QString noteRelativePathHelper(const NoteT* note, const QString& storedPath)
// {
//     const QString fileName = QFileInfo(storedPath).fileName();
//     const QString nameToUse = fileName.isEmpty() ? storedPath : fileName;

//     if (!note) {
//         return nameToUse;
//     }

//     const cwTrip* trip = note->parentTrip();
//     const cwCave* cave = trip ? trip->parentCave() : nullptr;
//     const cwCavingRegion* region = cave ? cave->parentRegion() : nullptr;
//     const cwProject* project = region ? region->parentProject() : nullptr;

//     if (project == nullptr || project->filename().isEmpty()) {
//         return QString();
//     }

//     const QDir projectDir = QFileInfo(project->filename()).absoluteDir();
//     const QString absolutePath = cwSaveLoad::dir(note).absoluteFilePath(nameToUse);
//     return projectDir.relativeFilePath(absolutePath);
// }

struct cwSaveLoad::Data {

    struct FileSystemJob {
        enum class Kind { File, Directory };
        enum class Action { Rename, Remove };

        const QObject* object;
        QString oldPath;
        QString newPath;
        Kind kind = Kind::File;
        Action action = Action::Rename;

        bool rename() const {
            // qDebug() << "Renaming: " << oldPath << "to" << newPath;
            return QDir().rename(oldPath, newPath);
        }

        bool remove() const {
            switch(kind) {
            case Kind::File:
                Q_ASSERT(QFileInfo::exists(oldPath));
                Q_ASSERT(QFileInfo(oldPath).isFile());
                return QFile::remove(oldPath);
            case Kind::Directory:
                Q_ASSERT(QFileInfo::exists(oldPath));
                Q_ASSERT(QFileInfo(oldPath).isDir());
                QDir dir(oldPath);
                // qDebug() << "Removing dir:" << dir;
                // return true;
                return dir.removeRecursively();
            }
        }

        bool execute() const {
            switch(action) {
            case Action::Rename:
                return rename();
                break;
            case Action::Remove:
                return remove();
                break;
            }
            return false;
        }

        static bool lessThan(const FileSystemJob& a, const FileSystemJob& b) {
            auto pathDepth = [](const QString& path) -> int {
                const QString cleaned = QDir::cleanPath(path);

                int slashCount = 0;
                for (const QChar ch : QStringView{cleaned}) {
                    if (ch == u'/') {
                        ++slashCount;
                    }
                }
                return slashCount;
            };

            auto kindRank = [](FileSystemJob::Kind kind) -> int {
                // Files first (0), directories second (1).
                return (kind == FileSystemJob::Kind::File) ? 0 : 1;
            };

            auto actionRank = [](FileSystemJob::Action action) -> int {
                return action == FileSystemJob::Action::Remove ? 0 : 1;
            };

            {
                const int aRank = actionRank(a.action);
                const int bRank = actionRank(b.action);
                if (aRank != bRank) {
                    return aRank < bRank;
                }
            }

            {
                const int aRank = kindRank(a.kind);
                const int bRank = kindRank(b.kind);
                if (aRank != bRank) {
                    return aRank < bRank;
                }
            }

            {
                const int aDepth = pathDepth(a.oldPath);
                const int bDepth = pathDepth(b.oldPath);
                if (aDepth != bDepth) {
                    // Deeper first.
                    return aDepth > bDepth;
                }
            }

            // Stable lexical tie-breaker.
            return a.oldPath < b.oldPath;
        }
    };

    //More messages,
    struct WaitingJob {
        AsyncFuture::Deferred<Monad::ResultBase> jobDeferred;
        std::unique_ptr<const google::protobuf::Message> message;

        WaitingJob() = default;
        WaitingJob(AsyncFuture::Deferred<Monad::ResultBase> job,
                   std::unique_ptr<const google::protobuf::Message>&& msg) :
            jobDeferred(job),
            message(std::move(msg))
        {

        }

        WaitingJob(WaitingJob&&) noexcept = default;
        WaitingJob& operator=(WaitingJob&&) noexcept = default;

        // Prevent copying
        WaitingJob(const WaitingJob&) = delete;
        WaitingJob& operator=(const WaitingJob&) = delete;
    };

    QQuickGit::GitRepository* repository;

    QString projectFileName;

    //Where the objects are currently being saved
    //This the absolute directory to the m_rootDir
    QHash<const void*, QString> m_fileLookup;

    //For watching when object data has changed
    cwRegionTreeModel* m_regionTreeModel;

    //New file future
    QFuture<void> newFileFuture;

    //Saving jobs
    QList<FileSystemJob> m_fileSystemJobs;
    std::unordered_map<QString, WaitingJob> m_waitingJobs;
    QHash<QString, QFuture<void>> m_runningJobs;

    bool isTemporary = true;
    bool saveEnabled = true;

    cwFutureManagerToken futureToken;

    //Helps watch if we already has objects connected, this is for debugging only
    cwUniqueConnectionChecker connectionChecker;

    //This is for bug checking, should be removed
    bool clearingWait = false;

    static QList<cwSurveyChunkData> fromProtoSurveyChunks(const google::protobuf::RepeatedPtrField<CavewhereProto::SurveyChunk> & protoList);

    static ResultBase ensurePathForFile(const QString& filePath) {
        QFileInfo fileInfo(filePath);
        QDir dir = fileInfo.dir();
        if (!dir.exists()) {
            bool success = dir.mkpath(".");
            if(success) {
                ResultBase();
            } else {
                ResultBase(QStringLiteral("Couldn't create directory:") + dir.absolutePath());
            }
        }
        return ResultBase();
    }

    void addFileSystemJob(const FileSystemJob& job) {
        // Q_ASSERT((job.kind == FileSystemJob::Kind::Directory) == QFileInfo(job.oldPath).isDir());
        m_fileSystemJobs.append(job);


        // const auto beginIt = m_fileSystemJobs.begin();
        // const auto endIt   = m_fileSystemJobs.end();

        // const auto it = std::lower_bound(beginIt, endIt, job, lessThan);

        // // If an element with the same oldPath already exists at this position, replace it.
        // // This should update with removes too
        // if (it != endIt && it->oldPath == job.oldPath) {
        //     *it = job;
        //     return;
        // }

        // //If we're removing directories
        // if(job.action == FileSystemJob::Action::Remove
        //     && job.kind == FileSystemJob::Kind::Directory)
        // {
        //     //Search for rename jobs that are a subdirectory

        //     auto isParentOf = [](const QDir& parent, const QDir& child) {
        //         QString parentPath = parent.canonicalPath();
        //         QString childPath  = child.canonicalPath();

        //         if (parentPath.isEmpty() || childPath.isEmpty()) {
        //             // canonicalPath() can fail if the directory doesn’t exist
        //             return false;
        //         }

        //         return childPath.startsWith(parentPath + QDir::separator());
        //     };

        //     QDir jobDir = QFileInfo(job.oldPath).absoluteDir();
        //     auto removeJobs = std::ranges::remove_if(m_fileSystemJobs,
        //                                              [jobDir, isParentOf](const auto& currentJob) {
        //                                                  auto currentDir = QFileInfo(currentJob.newPath).absoluteDir();
        //                                                  return isParentOf(jobDir, currentDir);
        //                                              });
        //     m_fileSystemJobs.erase(removeJobs.begin(), removeJobs.end());

        // }


        // Otherwise insert at the correct sorted position.
        // m_fileSystemJobs.insert(static_cast<int>(it - beginIt), job);
    }

    void resetFileLookup() {
        m_fileLookup.clear();

        auto addObjects = [this](auto objects) {
            for(const auto object : objects) {
                m_fileLookup[object] = absolutePath(object);
            }
        };

        addObjects(m_regionTreeModel->all<cwCave*>(QModelIndex(), &cwRegionTreeModel::cave));
        addObjects(m_regionTreeModel->all<cwTrip*>(QModelIndex(), &cwRegionTreeModel::trip));
        addObjects(m_regionTreeModel->all<cwNote*>(QModelIndex(), &cwRegionTreeModel::note));
    }


    QFuture<Monad::ResultBase> saveProtoMessage(
        cwSaveLoad* context,
        const QString& filename,
        std::unique_ptr<const google::protobuf::Message> message,
        const void* objectId
        );

    QFuture<Monad::ResultBase> saveProtoMessage(
        cwSaveLoad* context,
        const QDir& dir,
        const QString& filename,
        std::unique_ptr<const google::protobuf::Message> message,
        const void* objectId)
    {
        Q_ASSERT(message);
        return saveProtoMessage(context, dir.absoluteFilePath(filename), std::move(message), objectId);
    }

    void excuteFileSystemActions() {
        if(!m_fileSystemJobs.isEmpty() && m_runningJobs.isEmpty()) {

            // 1) Deduplicate by 'object', keeping the *last* occurrence
            {
                struct JobIdentity {
                    const QObject* object = nullptr;
                    FileSystemJob::Kind kind = FileSystemJob::Kind::File;

                    bool operator==(const JobIdentity&) const = default;
                };

                struct JobIdentityHash {
                    std::size_t operator()(const JobIdentity& id) const noexcept {
                        return qHash(qMakePair(reinterpret_cast<quintptr>(id.object),
                                               static_cast<int>(id.kind)));
                    }
                };

                std::unordered_set<JobIdentity, JobIdentityHash> seenObjects;
                QList<FileSystemJob> deduplicated;
                deduplicated.reserve(m_fileSystemJobs.size());

                for (const auto& job : std::views::reverse(m_fileSystemJobs)) {
                    JobIdentity id{job.object, job.kind};
                    if (seenObjects.insert(id).second) {
                        deduplicated.push_back(job); // collected in reverse order
                    } else {
                        qDebug() << "Rejecting:" << job.object << (int)job.kind;
                    }
                }

                // Restore original order of the kept "last" entries
                std::ranges::reverse(deduplicated);
                m_fileSystemJobs.swap(deduplicated);
            }

            // 2) Sort the jobs for safe execution ordering
            std::ranges::sort(m_fileSystemJobs, FileSystemJob::lessThan);

            auto setOldPath = [this](FileSystemJob& job) {
                // qDebug() << "SetOldPath:" << m_fileLookup[job.object] << job.object;
                Q_ASSERT(m_fileLookup.contains(job.object));
                // Q_ASSERT(QFileInfo::exists(m_fileLookup[job.object]));
                auto oldPath = m_fileLookup[job.object];

                if(job.kind == FileSystemJob::Kind::Directory) {
                    // qDebug() << "Dir:" << QFileInfo(oldPath).absoluteDir().canonicalPath() << QFileInfo(QFileInfo(oldPath).absoluteDir().canonicalPath());
                    Q_ASSERT(QFileInfo(QFileInfo(oldPath).absoluteDir().canonicalPath()).isDir());
                    oldPath = QFileInfo(oldPath).absoluteDir().canonicalPath();
                } else {
                    //Check that it's a file
                    Q_ASSERT(QFileInfo(oldPath).isFile());
                }

                job.oldPath = oldPath;
            };

            for(auto& job : m_fileSystemJobs) {
                //Resolve the oldPath
                setOldPath(job);

                bool success = job.execute();
                // qDebug() << "Success:" << job.newPath << success;

                // qDebug() << "CouldRename:" << couldRename;

                //Remove waiting job, because the should re-queue for save with the new name
                m_waitingJobs.erase(job.oldPath);
            }

            m_fileSystemJobs.clear();
        }
    }

    template<typename T>
    void saveObject(cwSaveLoad* context, const T* object) {
        if(saveEnabled) {
            // qDebug() << "Saving object:" << object << object->name() << dir(object);
            auto saveFuture = context->save(dir(object), object);

            // saveFuture.then(context, [context, object](const ResultBase& base) {
            //     if(!base.hasError()) {
            //         auto filename = context->absolutePath(object);
            //         Q_ASSERT(QFileInfo::exists(filename));
            //         qDebug() << "Updating lookup:" << object << filename;
            //         context->d->m_fileLookup[object] = filename;
            //     }
            // });
        }

        // QPointer<const T> ptr = object;

        // AsyncFuture::observe(saveFuture)
        //     .context(context, [context, saveFuture, ptr](){
        //         if(!saveFuture.result().hasError()) {
        //         }
        //     });
    }

    template<typename T>
    void renameDirectoryAndFile(cwSaveLoad* context, const T* object) {
        //We need to handle changing the directory and changing the name of the file
        // auto currentFileName = m_fileLookup.value(object);
        // QString folderName = QFileInfo(currentFileName).absolutePath();

        auto fileRename = Data::FileSystemJob {object, QString(), QString(), Data::FileSystemJob::Kind::File, Data::FileSystemJob::Action::Remove};
        auto folderRename = Data::FileSystemJob {object, QString(), dir(object).absolutePath(), Data::FileSystemJob::Kind::Directory};

        addFileSystemJob(fileRename);
        addFileSystemJob(folderRename);

        context->save(object);
    }
};

cwSaveLoad::~cwSaveLoad() = default;

namespace {

struct ProjectDestination {
    QString sanitizedBaseName;
    QString rootDirPath;
    QString projectFilePath;
};

Result<ProjectDestination> projectDestination(const QString& destinationUrl)
{
    QString localPath = cwGlobals::convertFromURL(destinationUrl);
    if (localPath.isEmpty()) {
        return Result<ProjectDestination>(QStringLiteral("Save location can't be empty."));
    }

    localPath = cwGlobals::addExtension(localPath, QStringLiteral("cw"));
    QFileInfo destinationInfo(localPath);
    if (!destinationInfo.isAbsolute()) {
        destinationInfo = QFileInfo(QDir::current(), localPath);
    }

    const QString baseName = destinationInfo.completeBaseName();
    if (baseName.isEmpty()) {
        return Result<ProjectDestination>(QStringLiteral("Save name can't be empty."));
    }

    const QString sanitizedBaseName = cwSaveLoad::sanitizeFileName(baseName);
    if (sanitizedBaseName.isEmpty()) {
        return Result<ProjectDestination>(QStringLiteral("Save name can't be empty."));
    }

    QDir destinationDirectory = destinationInfo.absoluteDir();

    QString rootDirPath;
    if (destinationDirectory.dirName() == sanitizedBaseName) {
        rootDirPath = destinationDirectory.absolutePath();
    } else {
        rootDirPath = destinationDirectory.absoluteFilePath(sanitizedBaseName);
    }

    QFileInfo rootInfo(rootDirPath);
    QDir parentDir = rootInfo.dir();
    if (!parentDir.exists()) {
        if (!QDir().mkpath(parentDir.absolutePath())) {
            return Result<ProjectDestination>(QStringLiteral("Cannot create directory '%1'.").arg(parentDir.absolutePath()));
        }
    }

    const QDir rootDir(rootDirPath);
    const QString projectFilePath = rootDir.absoluteFilePath(sanitizedBaseName + QStringLiteral(".cw"));

    return Result<ProjectDestination>({sanitizedBaseName, rootDirPath, projectFilePath});
}

ResultBase copyDirectoryRecursively(const QDir& sourceDir, const QDir& destinationDir)
{
    if (!sourceDir.exists()) {
        return ResultBase(QStringLiteral("Source directory '%1' does not exist.").arg(sourceDir.absolutePath()));
    }

    if (QFileInfo::exists(destinationDir.absolutePath())) {
        return ResultBase(QStringLiteral("Destination '%1' already exists.").arg(destinationDir.absolutePath()));
    }

    if (!QDir().mkpath(destinationDir.absolutePath())) {
        return ResultBase(QStringLiteral("Cannot create directory '%1'.").arg(destinationDir.absolutePath()));
    }

    const QFileInfoList entries = sourceDir.entryInfoList(QDir::NoDotAndDotDot
                                                          | QDir::AllEntries
                                                          | QDir::Hidden
                                                          | QDir::System);
    for (const QFileInfo& entry : entries) {
        const QString targetPath = destinationDir.absoluteFilePath(entry.fileName());
        if (entry.isDir()) {
            auto result = copyDirectoryRecursively(QDir(entry.absoluteFilePath()), QDir(targetPath));
            if (result.hasError()) {
                return result;
            }
        } else {
            if (QFileInfo::exists(targetPath) && !QFile::remove(targetPath)) {
                return ResultBase(QStringLiteral("Can't overwrite '%1'.").arg(targetPath));
            }
            if (!QFile::copy(entry.absoluteFilePath(), targetPath)) {
                return ResultBase(QStringLiteral("Can't copy '%1' to '%2'.").arg(entry.absoluteFilePath(), targetPath));
            }
        }
    }

    return ResultBase();
}

}

ResultBase cwSaveLoad::transferProjectTo(const QString& destinationFileUrl, ProjectTransferMode mode)
{
    const auto destinationResult = projectDestination(destinationFileUrl);
    if (destinationResult.hasError()) {
        return ResultBase(destinationResult.errorMessage());
    }

    const auto destination = destinationResult.value();

    const QString currentProjectFile = fileName();
    if (currentProjectFile.isEmpty()) {
        return ResultBase(QStringLiteral("Project does not have an associated file."));
    }

    const QString currentRootPath = projectDir().absolutePath();
    const QString targetRootPath = destination.rootDirPath;
    const QString normalizedCurrentRoot = QDir(currentRootPath).absolutePath();
    const QString normalizedTargetRoot = QDir(targetRootPath).absolutePath();
    const bool sameRoot = normalizedCurrentRoot == normalizedTargetRoot;

    if (!sameRoot && QFileInfo::exists(targetRootPath)) {
        return ResultBase(QStringLiteral("Destination folder '%1' already exists.").arg(targetRootPath));
    }

    setSaveEnabled(false);
    auto enableGuard = qScopeGuard([this]() { setSaveEnabled(true); });

    waitForFinished();

    if (!sameRoot) {
        if (mode == ProjectTransferMode::Move) {
            if (!QDir().rename(currentRootPath, targetRootPath)) {
                return ResultBase(QStringLiteral("Couldn't move project to '%1'.").arg(targetRootPath));
            }
        } else if (mode == ProjectTransferMode::Copy) {
            auto copyResult = copyDirectoryRecursively(QDir(currentRootPath), QDir(targetRootPath));
            if (copyResult.hasError()) {
                return copyResult;
            }
        } else {
            return ResultBase(QStringLiteral("Unknown ProjectTransferMode, this is a bug"));
        }
    }

    QDir targetRootDir(sameRoot ? normalizedCurrentRoot : normalizedTargetRoot);
    const QString sourceFileName = QFileInfo(currentProjectFile).fileName();
    QString sourceFilePath = targetRootDir.absoluteFilePath(sourceFileName);

    if (!QFileInfo::exists(sourceFilePath)) {
        const QString label = mode == ProjectTransferMode::Move ? QStringLiteral("Moved") : QStringLiteral("Copied");
        return ResultBase(QStringLiteral("%1 project file '%2' is missing.").arg(label, sourceFilePath));
    }

    const QString desiredFilePath = destination.projectFilePath;
    const QString desiredFileName = QFileInfo(desiredFilePath).fileName();
    const QString desiredPath = targetRootDir.absoluteFilePath(desiredFileName);

    if (sourceFilePath != desiredPath) {
        if (QFileInfo::exists(desiredPath) && !QFile::remove(desiredPath)) {
            return ResultBase(QStringLiteral("Couldn't overwrite '%1'.").arg(desiredPath));
        }
        if (!QFile::rename(sourceFilePath, desiredPath)) {
            return ResultBase(QStringLiteral("Couldn't rename project file to '%1'.").arg(desiredPath));
        }
        sourceFilePath = desiredPath;
    }

    setFileName(desiredFilePath);
    setTemporary(false);
    d->resetFileLookup();

    setSaveEnabled(true);
    enableGuard.dismiss();

    return ResultBase();
}

cwSaveLoad::cwSaveLoad(QObject *parent) :
    QObject(parent),
    d(std::make_unique<cwSaveLoad::Data>())
{
    d->m_regionTreeModel = new cwRegionTreeModel(this);
    d->repository = new QQuickGit::GitRepository(this);

    //Connect all for watching for saves
    connectTreeModel();
}

void cwSaveLoad::newProject()
{
    //Make sure we haven't requested a newProject already and we
    //aren't already waiting
    if(!d->newFileFuture.isRunning()) {
        //Disconnect all connections
        disconnectTreeModel();

        //Wait for jobs to complete
        auto future = completeSaveJobs();

        d->newFileFuture = future.then(this, [this]() {
            //Clear the current data
            auto region = d->m_regionTreeModel->cavingRegion();

            if(region) [[likely]] {
                region->clearCaves();
                if(m_undoStack) {
                    m_undoStack->clear();
                }

                //Rename the region
                const auto tempName = randomName();
                region->setName(tempName);

                //Create the temp directory
                auto tempDir = createTemporaryDirectory(tempName);

                //Save the project file
                saveCavingRegion(tempDir, region);

                //Connect all for watching for saves
                connectTreeModel();

                setTemporary(true);
                setFileName(tempDir.absoluteFilePath(regionFileName(region)));
            }
        });
    }
}

QString cwSaveLoad::fileName() const
{
    return d->projectFileName;
}

QFuture<ResultBase> cwSaveLoad::load(const QString &filename)
{
    // qDebug() << "---- Loading: " << filename;

    //Disconnect all connections
    disconnectTreeModel();

    auto oldJobs = completeSaveJobs();

    QFuture<ResultBase> future = oldJobs.then(this, [this, filename]() {
        //Find all the cave file
        auto regionDataFuture = cwSaveLoad::loadAll(filename);

        d->futureToken.addJob({QFuture<void>(regionDataFuture), QStringLiteral("Loading")});

        return AsyncFuture::observe(regionDataFuture)
            .context(this, [this, regionDataFuture, filename]() {
                return mbind(regionDataFuture, [this, regionDataFuture, filename](const ResultBase&) {
                    // setTemporaryProject(false);
                    //The filename needs to be set first because, image providers should
                    //have the filename before the region model is set
                    setFileName(filename);
                    setTemporary(false);

                    setSaveEnabled(false);
                    d->m_regionTreeModel->cavingRegion()->setData(regionDataFuture.result().value());

                    // d->projectFileName = filename;

                    d->resetFileLookup();

                    setSaveEnabled(true);

                    connectTreeModel();

                    return ResultBase();
                });
            }).future();
    }).unwrap();

    return future;
}

void cwSaveLoad::setFileName(const QString &filename, bool initRepository)
{
    //This should load the filename


    if(d->projectFileName != filename) {
        d->projectFileName = filename;

        if(initRepository) {
            d->repository->setDirectory(QFileInfo(filename).absoluteDir());
            d->repository->initRepository();
            cw::git::ensureGitIgnoreHasCacheEntry(d->repository->directory());
        }

        emit fileNameChanged();
    }
}


void cwSaveLoad::setCavingRegion(cwCavingRegion *region)
{
    d->m_regionTreeModel->setCavingRegion(region);
}

const cwCavingRegion *cwSaveLoad::cavingRegion() const
{
    return d->m_regionTreeModel->cavingRegion();
}

void cwSaveLoad::setSaveEnabled(bool enabled)
{
    d->saveEnabled = enabled;
}

QQuickGit::GitRepository *cwSaveLoad::repository() const
{
    return d->repository;
}

cwFutureManagerToken cwSaveLoad::futureManagerToken() const
{
    return d->futureToken;
}

void cwSaveLoad::setFutureManagerToken(const cwFutureManagerToken &futureManagerToken)
{
    d->futureToken = futureManagerToken;
}

void cwSaveLoad::setUndoStack(QUndoStack *undoStack)
{
    if(m_undoStack != undoStack) {
        m_undoStack = undoStack;
    }
}

QFuture<ResultBase> cwSaveLoad::saveCavingRegion(const QDir &dir, const cwCavingRegion *region)
{
    return saveProtoMessage(dir,
                            regionFileName(region),
                            toProtoCavingRegion(region),
                            region);
}

std::unique_ptr<CavewhereProto::CavingRegion> cwSaveLoad::toProtoCavingRegion(const cwCavingRegion *region)
{
    auto protoRegion = std::make_unique<CavewhereProto::CavingRegion>();
    protoRegion->set_version(cwRegionIOTask::protoVersion());
    cwRegionSaveTask::saveString(protoRegion->mutable_cavewhereversion(), CavewhereVersion);
    cwRegionSaveTask::saveString(protoRegion->mutable_name(), region->name());
    return protoRegion;
}

QString cwSaveLoad::regionFileName(const QDir &dir, const cwCavingRegion *region)
{
    return dir.absoluteFilePath(sanitizeFileName(QStringLiteral("%1.cw").arg(region->name())));
}

void cwSaveLoad::save(const cwCave *cave)
{
    d->saveObject(this, cave);
}

void cwSaveLoad::save(const cwTrip *trip)
{
    d->saveObject(this, trip);
}

void cwSaveLoad::save(const cwNote* note)
{
    d->saveObject(this, note);
}

QFuture<ResultBase> cwSaveLoad::save(const QDir &dir, const cwCave *cave)
{
    return saveProtoMessage(
        dir,
        fileName(cave),
        toProtoCave(cave),
        cave);
}

std::unique_ptr<CavewhereProto::Cave> cwSaveLoad::toProtoCave(const cwCave *cave)
{
    auto protoCave = std::make_unique<CavewhereProto::Cave>();
    auto fileVersion = protoCave->mutable_fileversion();
    fileVersion->set_version(cwRegionIOTask::protoVersion());
    cwRegionSaveTask::saveString(fileVersion->mutable_cavewhereversion(), CavewhereVersion);
    *(protoCave->mutable_name()) = cave->name().toStdString();
    return protoCave;
}

QFuture<ResultBase> cwSaveLoad::save(const QDir &dir, const cwTrip *trip)
{
    return saveProtoMessage(
        dir,
        fileName(trip),
        toProtoTrip(trip),
        trip);
}

std::unique_ptr<CavewhereProto::Trip> cwSaveLoad::toProtoTrip(const cwTrip *trip)
{
    //Copy trip data into proto, on the main thread
    auto protoTrip = std::make_unique<CavewhereProto::Trip>();
    auto fileVersion = protoTrip->mutable_fileversion();
    fileVersion->set_version(cwRegionIOTask::protoVersion());
    cwRegionSaveTask::saveString(fileVersion->mutable_cavewhereversion(), CavewhereVersion);

    *(protoTrip->mutable_name()) = trip->name().toStdString();

    // cwRegionSaveTask::saveString(protoTrip->mutable_name(), trip->name());
    cwRegionSaveTask::saveDate(protoTrip->mutable_date(), trip->date().date());
    cwRegionSaveTask::saveTripCalibration(protoTrip->mutable_tripcalibration(), trip->calibrations());

    if(trip->team()->rowCount() > 0) {
        cwRegionSaveTask::saveTeam(protoTrip->mutable_team(), trip->team());
    }

    foreach(cwSurveyChunk* chunk, trip->chunks()) {
        CavewhereProto::SurveyChunk* protoChunk = protoTrip->add_chunks();
        cwRegionSaveTask::saveSurveyChunk(protoChunk, chunk);
    }

    return protoTrip;
}

// Keep your original behavior: PDFs are detected but handled separately.
inline bool isPDF(const QString& path) {
    if (cwPDFConverter::isSupported()) {
        QFileInfo info(path);
        return info.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0;
    }
    return false;
}

struct CopyCommand {
    QString sourceFilePath;
    QString destinationFilePath;
};

static QString uniqueDestinationPath(const QDir& destinationDirectory,
                                     const QString& sourceFilePath,
                                     const QSet<QString>& reservedPaths,
                                     const QHash<QString, QFuture<void>>& runningJobs)
{
    const QFileInfo sourceInfo(sourceFilePath);
    const QString baseName = sourceInfo.completeBaseName();
    const QString suffix = sourceInfo.suffix();

    auto isTaken = [&reservedPaths, &runningJobs](const QString& path) {
        return QFileInfo::exists(path) || reservedPaths.contains(path) || runningJobs.contains(path);
    };

    QString candidate = destinationDirectory.absoluteFilePath(sourceInfo.fileName());
    int counter = 1;
    while (isTaken(candidate)) {
        const QString numberedName = suffix.isEmpty()
            ? QStringLiteral("%1-%2").arg(baseName).arg(counter)
            : QStringLiteral("%1-%2.%3").arg(baseName).arg(counter).arg(suffix);
        candidate = destinationDirectory.absoluteFilePath(numberedName);
        ++counter;
    }

    return candidate;
}


// ------------------------
// Generic copy-and-emit pipe
// ------------------------
template<typename ResultType, typename MakeResultFunc>
void cwSaveLoad::copyFilesAndEmitResults(const QList<QString>& sourceFilePaths,
                                         const QDir& destinationDirectory,
                                         MakeResultFunc makeResult,
                                         std::function<void (QList<ResultType>)> outputCallBackFunc)
{
    const QDir rootDirectory = projectDir();
    Q_ASSERT(rootDirectory.exists());
    // qDebug() << "RootDir:" << rootDirectory;

    auto copyOne = [rootDirectory](const QString& sourceFilePath, const QString& destinationFilePath) {
        return mbind(Data::ensurePathForFile(destinationFilePath),
                     [rootDirectory, sourceFilePath, destinationFilePath](const ResultBase&) {
                         const bool success = QFile::copy(sourceFilePath, destinationFilePath);
                         if (success) {
                             return Monad::ResultString(rootDirectory.relativeFilePath(destinationFilePath));
                         } else {
                             qWarning() << "Can't copy!" << sourceFilePath << "->" << destinationFilePath;
                             return Monad::ResultString(QStringLiteral("Can't copy ")
                                                            + sourceFilePath + QStringLiteral(" ")
                                                            + destinationFilePath,
                                                        Monad::ResultBase::Unknown);
                         }
                     });
    };

    QList<CopyCommand> commands;
    commands.reserve(sourceFilePaths.size());
    QSet<QString> reservedPaths;
    reservedPaths.reserve(sourceFilePaths.size());
    for (const QString& source : sourceFilePaths) {
        const QString destination = uniqueDestinationPath(destinationDirectory,
                                                          source,
                                                          reservedPaths,
                                                          d->m_runningJobs);
        if (!d->m_runningJobs.contains(destination)) {
            commands.append(CopyCommand{source, destination});
            reservedPaths.insert(destination);
        } else {
            qWarning() << "Can't add" << source << "because it's already queued" << LOCATION;
        }
    }

    auto future = AsyncFuture::observe(d->newFileFuture)
                      .context(this, [copyOne, commands]() {
                          return cwConcurrent::mapped(commands, [copyOne](const CopyCommand& command) {
                              return copyOne(command.sourceFilePath, command.destinationFilePath);
                          });
                      }).future();


    for (const CopyCommand& command : commands) {
        // qDebug() << "Adding files:" << sourceFilePaths << command.destinationFilePath;
        d->m_runningJobs.insert(command.destinationFilePath, QFuture<void>(future));
    }

    auto callBackFuture = AsyncFuture::observe(future)
                              .context(this, [this, future, commands, rootDirectory, makeResult, outputCallBackFunc]() {
                                  for (const CopyCommand& command : commands) {
                                      d->m_runningJobs.remove(command.destinationFilePath);
                                  }

                                  const auto results = future.results(); // QList<Monad::Result<QString>> of relative paths

                                  QList<ResultType> finalResults;
                                  finalResults.reserve(results.size());

                                  std::transform(results.begin(), results.end(),
                                                 std::back_inserter(finalResults),
                                                 [makeResult](const Monad::Result<QString>& relativePathResult) {
                                                     if (!relativePathResult.hasError()) {
                                                         return makeResult(relativePathResult.value()); // ResultType
                                                     } else {
                                                         qWarning() << "Error:" << relativePathResult.errorMessage() << LOCATION;
                                                         return ResultType{};
                                                     }
                                                 });

                                  outputCallBackFunc(finalResults);
                              }).future();

    d->futureToken.addJob(cwFuture(QFuture<void>(callBackFuture),
                                   QStringLiteral("Adding files")));
}

// ------------------------
// Specializations for cwImage and QString
// ------------------------
static auto makeImageFromRelativePath(const QDir& rootDirectory) {
    return [rootDirectory](const QString& relativePath) -> cwImage {
        const QString absolutePath = rootDirectory.absoluteFilePath(relativePath);
        cwImage image;
        image.setOriginalImageInfo(imageInfo(absolutePath));
        const QFileInfo fileInfo(relativePath);
        image.setPath(fileInfo.fileName());
        // qDebug() << "Returning image:" << image;
        return image;
    };
}

static auto makeRelativePathEcho() {
    return [](const QString& relativePath) -> QString {
        return relativePath;
    };
}

// ------------------------
// addImages: now uses the generic pipeline
// ------------------------
void cwSaveLoad::addImages(QList<QUrl> noteImagePaths,
                           const QDir& dir,
                           std::function<void (QList<cwImage>)> outputCallBackFunc)
{
    QVector<QString> imageFilePaths;
    QVector<QString> pdfFilePaths;
    imageFilePaths.reserve(noteImagePaths.size());
    pdfFilePaths.reserve(noteImagePaths.size());

    for (const QUrl& url : noteImagePaths) {
        const QString path = url.toLocalFile();
        if (isPDF(path)) {
            pdfFilePaths.append(path);
        } else {
            imageFilePaths.append(path);
        }
    }

    // Always process normal images first (original behavior).
    {
        const QDir rootDirectory = projectDir();
        copyFilesAndEmitResults<cwImage>(
            imageFilePaths,
            dir,
            makeImageFromRelativePath(rootDirectory),
            outputCallBackFunc
            );
    }

    // Optional: process PDFs into images, then emit. Kept separate to preserve ordering and avoid surprises.
    if (!pdfFilePaths.isEmpty() && cwPDFConverter::isSupported()) {
#ifdef CW_WITH_PDF_SUPPORT
        const QDir rootDirectory = projectDir();

        auto makeImagesFromPdf = [rootDirectory](const QString& relativePath) {
            QList<cwImage> images;
            const QString absolutePath = rootDirectory.absoluteFilePath(relativePath);

            QPdfDocument document;
            if (document.load(absolutePath) != QPdfDocument::Error::None) {
                qWarning() << "Failed to load PDF:" << absolutePath;
                return images;
            }

            const int pageCount = document.pageCount();
            const QString fileName = QFileInfo(relativePath).fileName();

            images.reserve(pageCount);

            for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex) {
                cwImage image;
                image.setPath(fileName);
                image.setOriginalImageInfo(imageInfo(document, pageIndex));
                image.setPage(pageIndex);
                images.append(image);
            }

            return images;
        };

        copyFilesAndEmitResults<QString>(
            pdfFilePaths,
            dir,
            makeRelativePathEcho(),
            [makeImagesFromPdf, outputCallBackFunc](QList<QString> relativePaths) {
                QList<cwImage> allImages;
                for (const QString& relativePath : relativePaths) {
                    allImages.append(makeImagesFromPdf(relativePath));
                }
                if (!allImages.isEmpty()) {
                    outputCallBackFunc(allImages);
                }
            }
            );
#else
        qWarning() << "PDF support not enabled for cwSaveLoad::addImages";
#endif
    }
}

ResultBase cwSaveLoad::moveProjectTo(const QString& destinationFileUrl)
{
    return transferProjectTo(destinationFileUrl, ProjectTransferMode::Move);
}

ResultBase cwSaveLoad::copyProjectTo(const QString& destinationFileUrl)
{
    return transferProjectTo(destinationFileUrl, ProjectTransferMode::Copy);
}

ResultBase cwSaveLoad::deleteTemporaryProject()
{
    if (!isTemporaryProject()) {
        return ResultBase(QStringLiteral("Project is not temporary."));
    }

    waitForFinished();

    QDir dir = projectDir();
    if (dir.exists()) {
        qDebug() << "Wanting to delete:" << dir;
        // if (!dir.removeRecursively()) {
        //     return ResultBase(QStringLiteral("Couldn't delete temporary project at '%1'.").arg(dir.absolutePath()));
        // }
    }

    return ResultBase();
}

// ------------------------
// addFiles: generic file ingest that returns relative paths
// ------------------------
void cwSaveLoad::addFiles(QList<QUrl> files,
                          const QDir& dir,
                          std::function<void (QList<QString>)> fileCallBackFunc)
{
    QList<QString> sourceFilePaths;
    sourceFilePaths.reserve(files.size());
    for (const QUrl& url : files) {
        sourceFilePaths.append(url.toLocalFile());
    }

    // For generic files, we only copy and return the relative destination paths.
    const QDir rootDirectory = projectDir();
    copyFilesAndEmitResults<QString>(
        sourceFilePaths,
        dir,
        makeRelativePathEcho(),
        fileCallBackFunc
        );
}


// void cwSaveLoad::addImages(QList<QUrl> noteImagePaths,
//                            const QDir &dir,
//                            std::function<void (QList<cwImage>)> outputCallBackFunc)
// {
//     auto isPDF = [](const QString& path) {
//         if(cwPDFConverter::isSupported()) {
//             QFileInfo info(path);
//             return info.suffix().compare("pdf", Qt::CaseInsensitive) == 0;
//         }
//         return false;
//     };

//     //Sort by images and pdf, pdf's last, but them in the same image order
//     QVector<QString> images;
//     QVector<QString> pdfs;
//     images.reserve(noteImagePaths.size());
//     pdfs.reserve(noteImagePaths.size());
//     for(const QUrl& url : noteImagePaths) {
//         QString path = url.toLocalFile();
//         if(isPDF(path)) {
//             pdfs.append(path);
//         } else {
//             //Normal image
//             images.append(path);
//         }
//     }

//     auto addImagesByPath = [this, dir, outputCallBackFunc](const QList<QString>& images) {
//         auto destFileName = [dir](const QString& fileName) {
//             return dir.absoluteFilePath(QFileInfo(fileName).fileName());
//         };

//         auto rootDir = projectDir();
//         Q_ASSERT(rootDir.exists());
//         qDebug() << "RootDir:" << rootDir;

//         auto copyFileToNote = [rootDir](const QString& fileName, const QString& newFileName) {
//             return mbind(Data::ensurePathForFile(newFileName), [rootDir, fileName, newFileName](const ResultBase& result) {
//                 bool success = QFile::copy(fileName, newFileName);
//                 if(success) {
//                     QImage qimage(fileName);
//                     cwImage image = cwAddImageTask::originalMetaData(qimage);
//                     // cwImage image;
//                     image.setPath(rootDir.relativeFilePath(newFileName));

//                     return Monad::Result<cwImage>(image);
//                 } else {
//                     qWarning() << "Can't copy!";
//                     return Monad::Result<cwImage>(QStringLiteral("Can't copy ") + fileName + QStringLiteral(" ") + newFileName);
//                 }
//             });
//         };

//         struct CopyCommand {
//             QString oldName;
//             QString newName;
//         };

//         auto commands = [destFileName, images, this]() {
//             QList<CopyCommand> commands;
//             commands.reserve(images.size());
//             for(const auto imagePath : images) {
//                 const auto newName = destFileName(imagePath);
//                 if(!d->m_runningJobs.contains(newName)) {
//                     commands.append({imagePath, newName});
//                 } else {
//                     qWarning() << "Can't add" << imagePath << " because it's already queued" << LOCATION;
//                 }
//             }
//             return commands;
//         }();

//         auto future = AsyncFuture::observe(d->newFileFuture)
//                           .context(this, [copyFileToNote, commands]() {
//                               return cwConcurrent::mapped(commands, [copyFileToNote](const CopyCommand& command) {
//                                   return copyFileToNote(command.oldName, command.newName);
//                               });

//                           }).future();


//         qDebug() << "Adding images:" << images;

//         for(const auto command : commands) {
//             d->m_runningJobs.insert(command.newName, QFuture<void>(future));
//         }

//         auto callBackFuture
//             = AsyncFuture::observe(future)
//                   .context(this, [future, commands, this, outputCallBackFunc]() {
//                       qDebug() << "Done!";

//                       for(const auto command : commands) {
//                           d->m_runningJobs.remove(command.newName);
//                       }

//                       const auto results = future.results();

//                       QList<cwImage> images;
//                       images.reserve(results.size());
//                       std::transform(results.begin(), results.end(),
//                                      std::back_inserter(images),
//                                      [](const Monad::Result<cwImage>& image)
//                                      {
//                                          if(!image.hasError()) {
//                                              qDebug() << "Returning image:" << image.value();
//                                              return image.value();
//                                          } else {
//                                              qWarning() << "Error:" << image.errorMessage() << LOCATION;
//                                              return cwImage();
//                                          }
//                                      });

//                       outputCallBackFunc(images);
//                   }).future();

//         d->futureToken.addJob(cwFuture(QFuture<void>(callBackFuture), QStringLiteral("Adding images")));
//     };

//     addImagesByPath(images);
// }

// void cwSaveLoad::addFiles(QList<QUrl> files, const QDir &dir, std::function<void (QList<QString>)> fileCallBackFunc)
// {

// }



QFuture<ResultBase> cwSaveLoad::save(const QDir &dir, const cwNote *note)
{
    return saveProtoMessage(
        dir,
        fileName(note),
        toProtoNote(note),
        note);
}

std::unique_ptr<CavewhereProto::Note> cwSaveLoad::toProtoNote(const cwNote *note)
{
    //Copy trip data into proto, on the main thread
    auto protoNote = std::make_unique<CavewhereProto::Note>();
    auto fileVersion = protoNote->mutable_fileversion();
    fileVersion->set_version(cwRegionIOTask::protoVersion());
    cwRegionSaveTask::saveString(fileVersion->mutable_cavewhereversion(), CavewhereVersion);

    cwRegionSaveTask::saveImage(protoNote->mutable_image(), note->image());

    protoNote->set_rotation(note->rotate());
    cwRegionSaveTask::saveImageResolution(protoNote->mutable_imageresolution(), note->imageResolution());

    foreach(cwScrap* scrap, note->scraps()) {
        CavewhereProto::Scrap* protoScrap = protoNote->add_scraps();
        cwRegionSaveTask::saveScrap(protoScrap, scrap);
    }

    *(protoNote->mutable_name()) = note->name().toStdString();

    return protoNote;
}

void cwSaveLoad::save(const cwNoteLiDAR *note)
{
    d->saveObject(this, note);
}

QFuture<ResultBase> cwSaveLoad::save(const QDir &dir, const cwNoteLiDAR *note)
{
    return saveProtoMessage(
        dir,
        fileName(note),
        toProtoNoteLiDAR(note),
        note);
}

std::unique_ptr<CavewhereProto::NoteLiDAR> cwSaveLoad::toProtoNoteLiDAR(const cwNoteLiDAR *note)
{
    //Copy trip data into proto, on the main thread
    auto protoNote = std::make_unique<CavewhereProto::NoteLiDAR>();
    auto fileVersion = protoNote->mutable_fileversion();
    fileVersion->set_version(cwRegionIOTask::protoVersion());
    cwRegionSaveTask::saveString(fileVersion->mutable_cavewhereversion(), CavewhereVersion);

    // cwRegionSaveTask::saveImage(protoNote->mutable_image(), note->image());

    // protoNote->set_rotation(note->rotate());
    // cwRegionSaveTask::saveImageResolution(protoNote->mutable_imageresolution(), note->imageResolution());

    // foreach(cwScrap* scrap, note->scraps()) {
    //     CavewhereProto::Scrap* protoScrap = protoNote->add_scraps();
    //     cwRegionSaveTask::saveScrap(protoScrap, scrap);
    // }

    *(protoNote->mutable_name()) = note->name().toStdString();
    *(protoNote->mutable_filename()) = note->filename().toStdString();

    protoNote->set_autocalculatenorth(note->autoCalculateNorth());

    for(const auto& noteStation : note->stations()) {
        //Save the note stations
        auto protoNoteStation = protoNote->add_notestations();
        *(protoNoteStation->mutable_name()) = noteStation.name().toStdString();
        cwRegionSaveTask::saveVector3D(protoNoteStation->mutable_positiononnote(), noteStation.positionOnNote());
    }

    saveNoteLiDARTranformation(protoNote->mutable_notetransformation(), note->noteTransformation());

    return protoNote;
}


/**
 * This saves all the data in region into directory
 *
 * This is useful for converting older CaveWhere files to the new file format
 */
QFuture<ResultString> cwSaveLoad::saveAllFromV6(
    const QDir &dir,
    const cwProject* project,
    const QString& projectFileName)
{
    auto makeDir = [](const QDir& dir) {
        dir.mkpath(QStringLiteral("."));
        return dir;
    };

    auto saveNoteImage = [projectFileName, dir](cwNoteData noteData, int imageIndex, QDir noteDir) {
        cwImageProvider provider;
        provider.setProjectPath(projectFileName);

        cwNote noteCopy;
        noteCopy.setData(noteData);

        if(noteCopy.name().isEmpty()) {
            noteCopy.setName(QString::number(imageIndex));
        }
        auto imageData = provider.data(noteCopy.image().original());

        auto filename = noteDir.absoluteFilePath(QStringLiteral("%1.%2")
                                                     .arg(imageIndex)
                                                     .arg(imageData.format().toLower()));



        QSaveFile file(filename);
        file.open(QSaveFile::WriteOnly);
        file.write(imageData.data());
        file.commit();

        // qDebug() << "Saving image:" << filename << file.errorString();

        cwImage noteImage = noteCopy.image();
        QString relativeFilename = dir.relativeFilePath(filename);
        noteImage.setPath(relativeFilename);
        noteCopy.setImage(noteImage);

        return noteCopy.data();
    };


    auto saveNotes = [makeDir, this, dir, saveNoteImage](const QDir& tripDir, const cwSurveyNoteModel* notes) {
        const QDir noteDir = makeDir(noteDirHelper(tripDir));

        QList<QFuture<ResultBase>> noteFutures;
        noteFutures.reserve(notes->notes().size() * 2);

        int imageIndex = 1;
        for(const cwNote* note : notes->notes()) {

            auto noteData = note->data();

            auto saveImageFuture = cwConcurrent::run([saveNoteImage, noteData, imageIndex, noteDir]() {
                return saveNoteImage(noteData, imageIndex, noteDir);
            });

            auto noteFuture =
                AsyncFuture::observe(saveImageFuture)
                    .context(this, [this, noteDir, noteData, saveImageFuture]() {
                        cwNote noteCopy;
                        noteCopy.setData(saveImageFuture.result());

                        return save(noteDir, &noteCopy);
                    }).future();


            noteFutures.append(noteFuture);

            ++imageIndex;
        }

        return noteFutures;
    };

    auto saveTrips = [this, projectFileName, makeDir, saveNotes](const QDir& caveDir, const cwCave* cave) {
        QList<QFuture<ResultBase>> tripFutures;
        tripFutures.reserve(cave->tripCount());

        QList<QFuture<ResultBase>> noteFutures;

        for(const auto trip : cave->trips()) {
            const QDir tripDir = makeDir(tripDirHelper(caveDir, trip));
            tripFutures.append(save(tripDir, trip));
            noteFutures.append(saveNotes(tripDir, trip->notes()));
        }

        return tripFutures + noteFutures;
    };

    //Save the region's data
    cwCavingRegion region;
    region.setName(QFileInfo(project->filename()).baseName());
    makeDir(dir);
    auto regionFuture = saveCavingRegion(dir, &region);
    QString newProjectFilename = regionFileName(dir, &region);

    QList<QFuture<ResultBase>> saveFutures;
    saveFutures.append(regionFuture);

    //Go through all the caves
    for(const auto cave : project->cavingRegion()->caves()) {
        const QDir caveDir = makeDir(caveDirHelper(dir, cave));
        saveFutures.append(save(caveDir, cave));
        saveFutures.append(saveTrips(caveDir, cave));
    }

    auto combine = AsyncFuture::combine() << saveFutures;
    return combine.context(this, [newProjectFilename]() {
                      return ResultString(newProjectFilename);
                  }).future();

}

QFuture<Monad::Result<cwCavingRegionData>> cwSaveLoad::loadAll(const QString &filename)
{
    return cwConcurrent::run([filename]() {
        // Load the root region file
        auto regionResult = loadCavingRegion(filename);

        return regionResult.then([filename](Result<cwCavingRegionData> result) {
            cwCavingRegionData region = result.value();

            QDir regionDir = QFileInfo(filename).absoluteDir();

            auto filePathLess = [](const QFileInfo& a, const QFileInfo& b) {
                return a.absoluteFilePath() < b.absoluteFilePath();
            };

            // Find all caves (*.cwcave)
            QFileInfoList caveFiles;
            QDirIterator it(regionDir.absolutePath(), QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                QDir caveDir(it.filePath());

                QFileInfoList files = caveDir.entryInfoList(QStringList() << "*.cwcave", QDir::Files);
                if (!files.isEmpty()) {
                    caveFiles.append(files);
                }
            }

            std::sort(caveFiles.begin(), caveFiles.end(), filePathLess);

            for (const QFileInfo &caveFileInfo : caveFiles) {
                auto caveResult = loadCave(caveFileInfo.absoluteFilePath());
                if (caveResult.hasError()) {
                    // FIXME: log or collect the error
                    qDebug() << "Cave result has errror:" << caveResult.errorMessage();
                    continue;
                }

                cwCaveData cave = caveResult.value();

                // Load all trips for this cave
                QDir caveDir = caveFileInfo.absoluteDir();
                QDir tripsDir(caveDir.filePath("trips"));
                if (tripsDir.exists()) {
                    QFileInfoList tripFiles;
                    QDirIterator tripIt(tripsDir.absolutePath(), QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
                    while (tripIt.hasNext()) {
                        tripIt.next();
                        QDir tripDir(tripIt.filePath());

                        QFileInfoList dirTripFiles = tripDir.entryInfoList(QStringList() << "*.cwtrip",
                                                                           QDir::Files,
                                                                           QDir::Name | QDir::IgnoreCase | QDir::LocaleAware);
                        tripFiles.append(dirTripFiles);
                    }

                    std::sort(tripFiles.begin(), tripFiles.end(), filePathLess);

                    for (const QFileInfo &tripFileInfo : tripFiles) {
                        auto tripResult = loadTrip(tripFileInfo.absoluteFilePath());

                        if (tripResult.hasError()) {
                            // FIXME: log or collect the error
                            continue;
                        }

                        cwTripData trip = tripResult.value();

                        QDir tripDir = tripFileInfo.absoluteDir();

                        auto loadObjectsFromNotesDir = [=](const QString& fileSuffix,
                                                           auto&& loadFunc,
                                                           auto& destinationList)
                        {
                            QDir notesDir = tripDir.filePath("notes");
                            if (!notesDir.exists()) {
                                return;
                            }

                            QFileInfoList files = notesDir.entryInfoList(QStringList() << ("*" + fileSuffix),
                                                                         QDir::Files,
                                                                         QDir::Name | QDir::IgnoreCase | QDir::LocaleAware);
                            std::sort(files.begin(), files.end(), filePathLess);
                            for (const QFileInfo& fileInfo : files) {
                                auto result = loadFunc(fileInfo.absoluteFilePath(), regionDir);
                                if (result.hasError()) {
                                    // FIXME: log or collect the error
                                    continue;
                                }

                                destinationList.append(result.value());
                            }
                        };

                        //Load 2D notes
                        loadObjectsFromNotesDir(QStringLiteral("cwnote"),
                                                loadNote,
                                                trip.noteModel.notes);

                        //Load 3D lidar notes
                        loadObjectsFromNotesDir(QStringLiteral("cwnote3d"),
                                                loadNoteLiDAR,
                                                trip.noteLiDARModel.notes);

                        cave.trips.append(trip);
                    }
                }

                region.caves.append(cave);
            }

            return Result(region);
        });
    });
}

Monad::Result<cwCavingRegionData> cwSaveLoad::loadCavingRegion(const QString &filename)
{    
    auto regionResult = loadMessage<CavewhereProto::CavingRegion>(filename);
    return Monad::mbind(regionResult, [](const Result<CavewhereProto::CavingRegion>& result)
                        {
                            auto regionProto = result.value();
                            cwCavingRegionData regionData;
                            if(regionProto.has_name()) {
                                regionData.name = QString::fromStdString(regionProto.name());
                            }
                            return Result(regionData);
                        });
}

Monad::Result<cwCaveData> cwSaveLoad::loadCave(const QString &filename)
{
    auto caveResult = loadMessage<CavewhereProto::Cave>(filename);
    return Monad::mbind(caveResult, [](const Result<CavewhereProto::Cave>& result)
                        {
                            auto caveProto = result.value();
                            cwCaveData caveData;
                            if(caveProto.has_name()) {
                                caveData.name = QString::fromStdString(caveProto.name());
                            }
                            return Result(caveData);
                        });
}

Monad::Result<cwTripData> cwSaveLoad::loadTrip(const QString &filename)
{
    auto tripResult = loadMessage<CavewhereProto::Trip>(filename);
    return Monad::mbind(tripResult, [](const Result<CavewhereProto::Trip>& result)
                        {
                            auto tripProto = result.value();
                            cwTripData tripData;

                            if(tripProto.has_name()) {
                                tripData.name = QString::fromStdString(tripProto.name());
                            }

                            if(tripProto.has_date()) {
                                tripData.date = QDateTime(cwRegionLoadTask::loadDate(tripProto.date()), QTime());
                            }

                            if(tripProto.has_tripcalibration()) {
                                tripData.calibrations = fromProtoTripCalibration(tripProto.tripcalibration());
                            }

                            if(tripProto.has_team()) {
                                tripData.team = fromProtoTeam(tripProto.team());
                            }

                            tripData.chunks = cwSaveLoad::Data::fromProtoSurveyChunks(tripProto.chunks());


                            return Result(tripData);
                        });
}

Monad::Result<cwNoteData> cwSaveLoad::loadNote(const QString &filename, const QDir& projectDir)
{
    auto noteResult = loadMessage<CavewhereProto::Note>(filename);

    return Monad::mbind(noteResult, [filename, projectDir](const Result<CavewhereProto::Note>& result) -> Monad::Result<cwNoteData> {
        const CavewhereProto::Note& protoNote = result.value();

        cwNoteData noteData;

        // Load rotation
        noteData.rotate = protoNote.rotation();

        // Load image resolution
        noteData.imageResolution = fromProtoImageResolution(protoNote.imageresolution());

        // Load image metadata, reloading from disk if legacy fields are missing.
        noteData.image = loadImage(protoNote.image(), filename);

        // Load scraps
        for (const auto& protoScrap : protoNote.scraps()) {
            auto scrap = fromProtoScrap(protoScrap);
            noteData.scraps.append(scrap);
        }

        noteData.name = QString::fromStdString(protoNote.name());

        return noteData;
    });
}

Monad::Result<cwNoteLiDARData> cwSaveLoad::loadNoteLiDAR(const QString& filename, const QDir &projectDir) {
    auto noteResult = loadMessage<CavewhereProto::NoteLiDAR>(filename);

    return Monad::mbind(noteResult, [filename, projectDir](const Result<CavewhereProto::NoteLiDAR>& result) -> Monad::Result<cwNoteLiDARData> {
        const CavewhereProto::NoteLiDAR& protoNote = result.value();

        cwNoteLiDARData noteData;

        const QString rawFilename = QString::fromStdString(protoNote.filename());
        // Older saves may contain project-relative paths; normalize to just the filename to stay resilient to renames.
        noteData.filename = QFileInfo(rawFilename).fileName().isEmpty() ? rawFilename : QFileInfo(rawFilename).fileName();
        noteData.name = QString::fromStdString(protoNote.name());

        noteData.stations.reserve(protoNote.notestations_size());
        for(const auto& protoNoteStation : protoNote.notestations()) {
            cwNoteLiDARStation newStation;
            newStation.setPositionOnNote(cwRegionLoadTask::loadVector3D(protoNoteStation.positiononnote()));
            newStation.setName(QString::fromStdString(protoNoteStation.name()));
            noteData.stations.append(std::move(newStation));
        }

        if(protoNote.has_autocalculatenorth()) {
            noteData.autoCalculateNorth = protoNote.autocalculatenorth();
        }

        if(protoNote.has_notetransformation()) {
            noteData.transfrom = fromProtoLiDARNoteTransformation(protoNote.notetransformation());
        }

        return noteData;
    });
}


cwTripCalibrationData cwSaveLoad::fromProtoTripCalibration(const CavewhereProto::TripCalibration &proto)
{
    cwTripCalibrationData tripCalibration;
    tripCalibration.setCorrectedCompassBacksight(proto.correctedcompassbacksight());
    tripCalibration.setCorrectedClinoBacksight(proto.correctedclinobacksight());
    tripCalibration.setCorrectedCompassFrontsight(proto.correctedcompassfrontsight());
    tripCalibration.setCorrectedClinoFrontsight(proto.correctedclinofrontsight());
    tripCalibration.setTapeCalibration(proto.tapecalibration());
    tripCalibration.setFrontCompassCalibration(proto.frontcompasscalibration());
    tripCalibration.setFrontClinoCalibration(proto.frontclinocalibration());
    tripCalibration.setBackCompassCalibration(proto.backcompassscalibration());
    tripCalibration.setBackClinoCalibration(proto.backclinocalibration());
    tripCalibration.setDeclination(proto.declination());
    tripCalibration.setDistanceUnit((cwUnits::LengthUnit)proto.distanceunit());
    tripCalibration.setFrontSights(proto.frontsights());
    tripCalibration.setBackSights(proto.backsights());
    return tripCalibration;
}

cwTeamData cwSaveLoad::fromProtoTeam(const CavewhereProto::Team &proto)
{
    QList<cwTeamMember> members;
    members.reserve(proto.teammembers_size());
    for(int i = 0; i < proto.teammembers_size(); i++) {
        cwTeamMember member = fromProtoTeamMember(proto.teammembers(i));
        members.append(member);
    }
    return {
        members
    };
}

cwTeamMember cwSaveLoad::fromProtoTeamMember(const CavewhereProto::TeamMember &proto)
{
    cwTeamMember member;
    auto id = toUuid(proto.id());
    if(!id.isNull()) {
        member.setId(id);
    }
    member.setJobs(fromProtoStringList(proto.jobs()));
    member.setName(QString::fromStdString(proto.name()));
    return member;
}

QList<cwSurveyChunkData> cwSaveLoad::Data::fromProtoSurveyChunks(const google::protobuf::RepeatedPtrField<CavewhereProto::SurveyChunk> &protoList)
{
    QList<cwSurveyChunkData> chunks;

    if(!protoList.empty()) {
        chunks.reserve(protoList.size());

        for (const auto& protoChunk : protoList) {
            chunks.append(cwSaveLoad::fromProtoSurveyChunk(protoChunk));
        }
    }

    return chunks;
}

cwSurveyChunkData cwSaveLoad::fromProtoSurveyChunk(const CavewhereProto::SurveyChunk &protoChunk)
{
    cwSurveyChunkData chunkData;
    chunkData.id = toUuid(protoChunk.id());

    const int legCount = protoChunk.leg_size();
    // Q_ASSERT(legCount % 2 == 0); // Each shot has 2 legs: a station and a shot

    for (int i = 0; i < legCount; i += 2) {
        const auto& protoStation = protoChunk.leg(i);
        chunkData.stations.append(fromProtoStation(protoStation));

        if(i + 1 < legCount) {
            const auto& protoShot = protoChunk.leg(i + 1);
            chunkData.shots.append(fromProtoShot(protoShot));
        }
    }

    // const int calibrationCount = protoChunk.calibrations_size();
    // for (int i = 0; i < calibrationCount; ++i) {
    //     const auto& protoCalibration = protoChunk.calibrations(i);
    //     int shotIndex = protoCalibration.shotindex();
    //     auto calibration = fromProtoTripCalibration(protoCalibration.calibration());
    //     chunkData.calibrations.insert(shotIndex, calibration);
    // }

    return chunkData;


}

cwStation cwSaveLoad::fromProtoStation(const CavewhereProto::StationShot &protoStation)
{
    cwStation station;

    station.setId(toUuid(protoStation.id()));

    if (protoStation.has_name()) {
        station.setName(QString::fromStdString(protoStation.name()));
    }

    if (protoStation.has_left()) {
        station.setLeft(QString::fromStdString(protoStation.left()));
    }

    if (protoStation.has_right()) {
        station.setRight(QString::fromStdString(protoStation.right()));
    }

    if (protoStation.has_up()) {
        station.setUp(QString::fromStdString(protoStation.up()));
    }

    if (protoStation.has_down()) {
        station.setDown(QString::fromStdString(protoStation.down()));
    }

    return station;
}

cwShot cwSaveLoad::fromProtoShot(const CavewhereProto::StationShot &protoShot)
{
    cwShot shot;

    shot.setId(toUuid(protoShot.id()));

    if (protoShot.has_includedistance()) {
        shot.setDistanceIncluded(protoShot.includedistance());
    }

    if (protoShot.has_distance()) {
        shot.setDistance(QString::fromStdString(protoShot.distance()));
    }

    if (protoShot.has_compass()) {
        shot.setCompass(QString::fromStdString(protoShot.compass()));
    }

    if (protoShot.has_backcompass()) {
        shot.setBackCompass(QString::fromStdString(protoShot.backcompass()));
    }

    if (protoShot.has_clino()) {
        shot.setClino(QString::fromStdString(protoShot.clino()));
    }

    if (protoShot.has_backclino()) {
        shot.setBackClino(QString::fromStdString(protoShot.backclino()));
    }

    return shot;
}

cwScrapData cwSaveLoad::fromProtoScrap(const CavewhereProto::Scrap &protoScrap)
{
    cwScrapData scrapData;

    if(protoScrap.has_id()) {
        scrapData.id = toUuid(protoScrap.id());
    }

    // Load outline points
    for (const QtProto::QPointF& protoPoint : protoScrap.outlinepoints()) {
        scrapData.outlinePoints.append(cwRegionLoadTask::loadPointF(protoPoint));
    }

    // Load stations
    for (const CavewhereProto::NoteStation& protoStation : protoScrap.notestations()) {
        scrapData.stations.append(fromProtoNoteStation(protoStation));
    }

    // Load leads
    for (const CavewhereProto::Lead& protoLead : protoScrap.leads()) {
        scrapData.leads.append(fromProtoLead(protoLead));
    }

    // Load note transformation
    scrapData.noteTransformation = fromProtoNoteTransformation(protoScrap.notetransformation());

    // Load calculate note transform flag
    scrapData.calculateNoteTransform = protoScrap.calculatenotetransform();

    //Generate the correct scrap type
    if(protoScrap.has_type()) {
        switch(protoScrap.type()) {
        case CavewhereProto::Scrap::ScrapType::Scrap_ScrapType_Plan:
            scrapData.viewMatrix = std::make_unique<cwPlanScrapViewMatrix::Data>();
            break;
        case CavewhereProto::Scrap::ScrapType::Scrap_ScrapType_RunningProfile:
            scrapData.viewMatrix = std::make_unique<cwRunningProfileScrapViewMatrix::Data>();
            break;
        case CavewhereProto::Scrap::ScrapType::Scrap_ScrapType_ProjectedProfile:
            // Load view matrix
            if (protoScrap.has_profileviewmatrix()) {
                scrapData.viewMatrix = fromProtoProjectedScraptViewMatrix(protoScrap.profileviewmatrix());
            } else {
                scrapData.viewMatrix = std::make_unique<cwProjectedProfileScrapViewMatrix::Data>();
            }
            break;
        default:
            scrapData.viewMatrix = std::make_unique<cwPlanScrapViewMatrix::Data>();
            break;
        }
    } else {
        scrapData.viewMatrix = std::make_unique<cwPlanScrapViewMatrix::Data>();
    }

    return scrapData;
}

cwNoteStation cwSaveLoad::fromProtoNoteStation(const CavewhereProto::NoteStation &protoNoteStation)
{
    cwNoteStation noteStation;
    noteStation.setName(QString::fromStdString(protoNoteStation.name()));
    noteStation.setPositionOnNote(cwRegionLoadTask::loadPointF(protoNoteStation.positiononnote()));
    return noteStation;
}

cwLead cwSaveLoad::fromProtoLead(const CavewhereProto::Lead &protoLead)
{
    cwLead lead;

    // Load position on note
    lead.setPositionOnNote(cwRegionLoadTask::loadPointF(protoLead.positiononnote()));

    // Load description if present
    if (protoLead.has_description()) {
        lead.setDescription(QString::fromStdString(protoLead.description()));
    }

    // Load size if present and valid
    if (protoLead.has_size()) {
        QSizeF size = cwRegionLoadTask::loadSizeF(protoLead.size());
        if (size.isValid()) {
            lead.setSize(size);
        }
    }

    // Load completed flag
    lead.setCompleted(protoLead.completed());

    return lead;
}

cwNoteTransformationData cwSaveLoad::fromProtoNoteTransformation(const CavewhereProto::NoteTranformation &protoNoteTransform)
{
    cwNoteTransformationData data;

    data.north = protoNoteTransform.northup();

    if (protoNoteTransform.has_scalenumerator()) {
        data.scale.scaleNumerator = fromProtoLength(protoNoteTransform.scalenumerator());
    }

    if (protoNoteTransform.has_scaledenominator()) {
        data.scale.scaleDenominator = fromProtoLength(protoNoteTransform.scaledenominator());
    }

    return data;
}

cwNoteLiDARTransformationData cwSaveLoad::fromProtoLiDARNoteTransformation(const CavewhereProto::NoteLiDARTransformation &protoNoteTransform)
{
    cwNoteLiDARTransformationData data;

    if(protoNoteTransform.has_plantransform()) {
        //Do a slice
        cwNoteTransformationData& base = data;
        base = fromProtoNoteTransformation(protoNoteTransform.plantransform());
    }

    if(protoNoteTransform.has_upsign()) {
        data.upSign = protoNoteTransform.upsign();
    }

    if(protoNoteTransform.has_upmode()) {
        data.upMode = static_cast<cwNoteLiDARTransformationData::UpMode>(protoNoteTransform.upmode());

        if(data.upMode == cwNoteLiDARTransformationData::UpMode::Custom
            && protoNoteTransform.has_upcustom()) {
            data.upRotation = fromProtoQuaternion(protoNoteTransform.upcustom());
        }
    }

    return data;
}

std::unique_ptr<cwProjectedProfileScrapViewMatrix::Data> cwSaveLoad::fromProtoProjectedScraptViewMatrix(const CavewhereProto::ProjectedProfileScrapViewMatrix protoViewMatrix)
{
    auto matrix = std::make_unique<cwProjectedProfileScrapViewMatrix::Data>();
    matrix->setAzimuth(protoViewMatrix.azimuth());
    matrix->setDirection(static_cast<cwProjectedProfileScrapViewMatrix::AzimuthDirection>(protoViewMatrix.direction()));
    return matrix;
}

cwImageResolution::Data cwSaveLoad::fromProtoImageResolution(const CavewhereProto::ImageResolution &protoImageResolution)
{
    cwImageResolution::Data resolution;
    resolution.value = protoImageResolution.value();
    resolution.unit = static_cast<cwUnits::ImageResolutionUnit>(protoImageResolution.unit());
    return resolution;
}

QQuaternion cwSaveLoad::fromProtoQuaternion(const QtProto::QQuaternion &protoQuaternion)
{
    return QQuaternion(protoQuaternion.scalar(), protoQuaternion.x(), protoQuaternion.y(), protoQuaternion.z());
}

void cwSaveLoad::saveNoteLiDARTranformation(CavewhereProto::NoteLiDARTransformation *protoNoteTransformation, cwNoteLiDARTransformation *noteTransformation)
{
    if (!protoNoteTransformation || !noteTransformation) { return; }

    //Save the base class
    cwRegionSaveTask::saveNoteTranformation(protoNoteTransformation->mutable_plantransform(), noteTransformation);

    // upMode (enum)
    // The enum values appear to align (Custom=0, XisUp=1, YisUp=2, ZisUp=3). If they differ,
    // add a mapping switch here instead of static_cast.
    protoNoteTransformation->set_upmode(
        static_cast<CavewhereProto::NoteLiDARTransformation_UpMode>(noteTransformation->upMode())
        );

    // upSign (float)
    protoNoteTransformation->set_upsign(noteTransformation->upSign());

    // upCustom (QtProto::QQuaternion) — only meaningful when mode is Custom; safe to always write.
    if(noteTransformation->upMode() == cwNoteLiDARTransformation::UpMode::Custom) {
        saveQQuaternion(protoNoteTransformation->mutable_upcustom(), noteTransformation->upCustom());
    }
}

void cwSaveLoad::saveQQuaternion(QtProto::QQuaternion *protoQuaternion, const QQuaternion &quaternion)
{
    if (!protoQuaternion) { return; }

    protoQuaternion->set_x(quaternion.x());
    protoQuaternion->set_y(quaternion.y());
    protoQuaternion->set_z(quaternion.z());
    protoQuaternion->set_scalar(quaternion.scalar());
}

cwLength::Data cwSaveLoad::fromProtoLength(const CavewhereProto::Length &protoLength)
{
    return {
        protoLength.unit(),
        protoLength.value()
    };
}

void cwSaveLoad::waitForFinished()
{
    cwFutureManagerModel model;
    auto saveJobs = completeSaveJobs();
    model.addJob(cwFuture(saveJobs, QString()));
    model.addJob(cwFuture(d->newFileFuture, QString()));
    model.waitForFinished();
}

void cwSaveLoad::disconnectTreeModel()
{
    //Disconnect from the tree model
    disconnect(d->m_regionTreeModel, nullptr, this, nullptr);

    d->connectionChecker.remove(d->m_regionTreeModel);

    //Disconnect all the objects watch in the tree
    disconnectObjects();
}

void cwSaveLoad::connectTreeModel()
{

    if(!d->connectionChecker.add(d->m_regionTreeModel)) {
        return;
    }

    //Connect when region has changed
    connect(d->m_regionTreeModel, &cwRegionTreeModel::rowsInserted,
            this, [this](const QModelIndex &parent, int first, int last) {
                for(int i = first; i <= last; i++) {
                    auto index = d->m_regionTreeModel->index(i, 0, parent);
                    switch(index.data(cwRegionTreeModel::TypeRole).toInt()) {
                    case cwRegionTreeModel::CaveType: {
                        auto cave = d->m_regionTreeModel->cave(index);
                        connectCave(cave);
                        save(cave);
                        break;
                    }
                    case cwRegionTreeModel::TripType: {
                        auto trip = d->m_regionTreeModel->trip(index);
                        connectTrip(trip);
                        save(trip);
                        break;
                    }
                    case cwRegionTreeModel::NoteType: {
                        auto note = d->m_regionTreeModel->note(index);
                        connectNote(note);
                        save(note);
                        break;
                    }
                    case cwRegionTreeModel::ScrapType: {
                        auto scrap = d->m_regionTreeModel->scrap(index);
                        connectScrap(scrap);
                        save(scrap->parentNote());
                        break;
                    }
                    case cwRegionTreeModel::NoteLiDARType: {
                        auto noteLiDAR = d->m_regionTreeModel->noteLiDAR(index);
                        connectNoteLiDAR(noteLiDAR);
                        save(noteLiDAR);
                        break;
                    }
                    default:
                        break;
                    }
                }
            });

    connect(d->m_regionTreeModel, &cwRegionTreeModel::rowsAboutToBeRemoved,
            this, [this](const QModelIndex &parent, int first, int last) {

                auto removeDirectory = [this](const QObject* object) {
                    d->addFileSystemJob(Data::FileSystemJob
                                        {
                                            object,
                                            // dir.canonicalPath(),
                                            QString(),
                                            QString(),
                                            Data::FileSystemJob::Kind::Directory,
                                            Data::FileSystemJob::Action::Remove
                                        });
                    d->excuteFileSystemActions();
                };

                for(int i = first; i <= last; i++) {
                    auto index = d->m_regionTreeModel->index(i, 0, parent);
                    auto object = index.data(cwRegionTreeModel::ObjectRole).value<QObject*>();

                    switch(index.data(cwRegionTreeModel::TypeRole).toInt()) {
                    case cwRegionTreeModel::CaveType: {
                        auto cave = d->m_regionTreeModel->cave(index);
                        // auto caveDir = dir(cave);
                        removeDirectory(cave);
                        break;
                    }
                    case cwRegionTreeModel::TripType: {
                        auto trip = d->m_regionTreeModel->trip(index);
                        // auto tripDir = dir(trip);
                        removeDirectory(trip);
                        break;
                    }
                    case cwRegionTreeModel::NoteType: {
                        auto note = d->m_regionTreeModel->note(index);
                        auto noteFilename = absolutePath(note);
                        // qDebug() << "Deleting:" << noteFilename << note << note->name();
                        d->addFileSystemJob(Data::FileSystemJob
                                            {
                                                note,
                                                QString(),
                                                QString(),
                                                Data::FileSystemJob::Kind::File,
                                                Data::FileSystemJob::Action::Remove
                                            });
                        d->excuteFileSystemActions();
                        break;
                    }
                    default:
                        break;
                    }

                    d->connectionChecker.remove(object);
                    disconnect(object, nullptr, this, nullptr);

                }
            });


    // connect(d->m_regionTreeModel, &cwRegionTreeModel::modelAboutToBeReset,
    //         this, [this]() {
    //     disconnectObjects();
    // });

    // auto reset = [this]() {
    //     //Connect all the subobjects
    //     connectObjects();

    //     //Save the current caving region,
    // };

    // connect(d->m_regionTreeModel, &cwRegionTreeModel::modelReset,
    //         this, reset);


    connectObjects();

    // reset();
}

void cwSaveLoad::disconnectObjects()
{
    //Disconnect from all the objects
    QList<QObject*> objects = d->m_regionTreeModel->all<QObject*>(QModelIndex(), &cwRegionTreeModel::object);
    for(auto obj : objects) {
        d->connectionChecker.remove(obj);
        disconnect(obj, nullptr, this, nullptr);
    }
}

void cwSaveLoad::connectObjects()
{

    {
        auto caves = d->m_regionTreeModel->all<cwCave*>(QModelIndex(), &cwRegionTreeModel::cave);
        for(auto cave : caves) {
            connectCave(cave);
        }
    }

    {
        auto trips = d->m_regionTreeModel->all<cwTrip*>(QModelIndex(), &cwRegionTreeModel::trip);
        for(auto trip : trips) {
            connectTrip(trip);
        }
    }

    {
        auto notes = d->m_regionTreeModel->all<cwNote*>(QModelIndex(), &cwRegionTreeModel::note);
        for(auto note : notes) {
            connectNote(note);
        }
    }

    {
        auto notes = d->m_regionTreeModel->all<cwNoteLiDAR*>(QModelIndex(), &cwRegionTreeModel::noteLiDAR);
        for(auto note : notes) {
            connectNoteLiDAR(note);
        }
    }

}

void cwSaveLoad::connectCave(cwCave *cave)
{
    if (cave == nullptr) {
        return;
    }

    auto saveCaveName = [cave, this]() {
        d->renameDirectoryAndFile(this, cave);
    };

    if(!d->connectionChecker.add(cave)) {
        return;
    }

    connect(cave, &cwCave::nameChanged, this, saveCaveName);
}


void cwSaveLoad::connectTrip(cwTrip* trip)
{
    if (trip == nullptr) {
        return;
    }

    // Lambda that saves this specific trip
    const auto saveTrip = [this, trip]() {
        save(trip);
    };

    // Helper to connect a survey chunk to save on any data change
    const auto connectChunk = [this, saveTrip](cwSurveyChunk* chunk) {
        if (chunk == nullptr) {
            return;
        }

        if(!d->connectionChecker.add(chunk)) {
            return;
        }

        connect(chunk, &cwSurveyChunk::added, this, saveTrip);
        connect(chunk, &cwSurveyChunk::aboutToRemove, this, saveTrip);
        connect(chunk, &cwSurveyChunk::removed, this, saveTrip);

        // connect(chunk, &cwSurveyChunk::stationsAdded, this, saveTrip);
        // connect(chunk, &cwSurveyChunk::shotsAdded, this, saveTrip);
        // connect(chunk, &cwSurveyChunk::stationsRemoved, this, saveTrip);
        // connect(chunk, &cwSurveyChunk::shotsRemoved, this, saveTrip);

        connect(chunk, &cwSurveyChunk::calibrationsChanged, this, saveTrip);
        // connect(chunk, &cwSurveyChunk::connectedChanged, this, saveTrip);
        // connect(chunk, &cwSurveyChunk::connectedStateChanged, this, saveTrip);
        // connect(chunk, &cwSurveyChunk::shotCountChanged, this, saveTrip);
        // connect(chunk, &cwSurveyChunk::stationCountChanged, this, saveTrip);

        connect(chunk, &cwSurveyChunk::dataChanged, this, saveTrip);
        // connect(chunk, &cwSurveyChunk::errorsChanged, this, saveTrip);
        // parentTripChanged intentionally not handled (no re-parenting)
    };

    if(!d->connectionChecker.add(trip)) {
        return;
    }

    auto saveTripName = [trip, this]() {
        d->renameDirectoryAndFile(this, trip);
    };
    connect(trip, &cwTrip::nameChanged, this, saveTripName);

    // Trip-level changes
    connect(trip, &cwTrip::dateChanged, this, saveTrip);
    // connect(trip, &cwTrip::numberOfChunksChanged, this, saveTrip);
    connect(trip, &cwTrip::chunksAboutToBeRemoved, this, saveTrip);
    connect(trip, &cwTrip::chunksRemoved, this, saveTrip);

    // Newly inserted chunks → connect them and save
    connect(trip, &cwTrip::chunksInserted, this,
            [this, trip, connectChunk, saveTrip](int begin, int end) {
                for (int i = begin; i <= end; ++i) {
                    if (auto* chunk = trip->chunk(i)) { // adjust if accessor differs
                        connectChunk(chunk);
                    }
                }
                saveTrip();
            });

    // Connect all existing chunks
    for (int i = 0; i < trip->chunkCount(); ++i) {
        if (auto* chunk = trip->chunk(i)) {
            connectChunk(chunk);
        }
    }

    // Team model changes → save
    if (QAbstractItemModel* const teamModel = trip->team()) {

        if(!d->connectionChecker.add(teamModel)) {
            return;
        }

        connect(teamModel, &QAbstractItemModel::dataChanged, this, saveTrip);
        connect(teamModel, &QAbstractItemModel::rowsInserted, this, saveTrip);
        connect(teamModel, &QAbstractItemModel::rowsRemoved, this, saveTrip);
        connect(teamModel, &QAbstractItemModel::modelReset, this, saveTrip);
        connect(teamModel, &QAbstractItemModel::layoutChanged, this, saveTrip);
    }

    // Notes model changes → save
    // Notes model is update through cwCavingRegionTree


    // Trip calibration changes → save
    if (cwTripCalibration* const cal = trip->calibrations()) {

        if(!d->connectionChecker.add(cal)) {
            return;
        }

        connect(cal, &cwTripCalibration::correctedCompassBacksightChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::correctedClinoBacksightChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::correctedCompassFrontsightChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::correctedClinoFrontsightChanged, this, saveTrip);

        connect(cal, &cwTripCalibration::tapeCalibrationChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::frontCompassCalibrationChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::frontClinoCalibrationChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::backCompassCalibrationChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::backClinoCalibrationChanged, this, saveTrip);

        connect(cal, &cwTripCalibration::declinationChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::distanceUnitChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::supportedUnitsChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::frontSightsChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::backSightsChanged, this, saveTrip);
    }

    // parentCaveChanged intentionally omitted (no re-parenting)
}



void cwSaveLoad::connectNote(cwNote *note)
{
    if (note == nullptr) {
        return;
    }

    auto renameAndSaveNote = [this, note]() {
        //Only rename if saved before
        // auto currentFileName = d->m_fileLookup.value(note);
        auto fileRename = Data::FileSystemJob {note, QString(), absolutePath(note), Data::FileSystemJob::Kind::File};
        d->addFileSystemJob(fileRename);

        save(note);
    };

    if(!d->connectionChecker.add(note)) {
        return;
    }

    // Note-level changes
    connect(note, &cwNote::nameChanged, this, renameAndSaveNote);

    // Lambda to save this specific note
    const auto saveNote = [this, note]() {
        save(note);
    };
    connect(note, &cwNote::imageChanged, this, saveNote);
    connect(note, &cwNote::rotateChanged, this, saveNote);
    connect(note->imageResolution(), &cwImageResolution::valueChanged, this, saveNote);
    connect(note->imageResolution(), &cwImageResolution::unitChanged, this, saveNote);

    // connect(note, &cwNote::beginInsertingScraps, this, saveNote);
    connect(note, &cwNote::insertedScraps, this, saveNote);
    // connect(note, &cwNote::beginRemovingScraps, this, saveNote);
    connect(note, &cwNote::removedScraps, this, saveNote);
    // connect(note, &cwNote::scrapAdded, this, saveNote);
    connect(note, &cwNote::scrapsReset, this, saveNote);

    //Connect scraps
    const auto scraps = note->scraps();
    for(auto scrap : scraps) {
        connectScrap(scrap);
    }
}

void cwSaveLoad::connectScrap(cwScrap *scrap)
{
    if (scrap == nullptr) {
        return;
    }

    // Lambda to save this specific note
    const auto saveNote = [this, scrap]() {
        save(scrap->parentNote());
    };

    if(!d->connectionChecker.add(scrap)) {
        return;
    }

    // Scrap outline changes
    connect(scrap, &cwScrap::insertedPoints, this, saveNote);
    connect(scrap, &cwScrap::removedPoints, this, saveNote);
    connect(scrap, &cwScrap::pointChanged, this, saveNote);
    connect(scrap, &cwScrap::pointsReset, this, saveNote);
    connect(scrap, &cwScrap::closeChanged, this, saveNote);

    // Scrap stations
    connect(scrap, &cwScrap::stationAdded, this, saveNote);
    connect(scrap, &cwScrap::stationPositionChanged, this, saveNote);
    connect(scrap, &cwScrap::stationNameChanged, this, saveNote);
    connect(scrap, &cwScrap::stationRemoved, this, saveNote);
    connect(scrap, &cwScrap::stationsReset, this, saveNote);

    // Scrap leads
    connect(scrap, &cwScrap::leadsBeginInserted, this, saveNote);
    connect(scrap, &cwScrap::leadsInserted, this, saveNote);
    connect(scrap, &cwScrap::leadsBeginRemoved, this, saveNote);
    connect(scrap, &cwScrap::leadsRemoved, this, saveNote);
    connect(scrap, &cwScrap::leadsDataChanged, this, saveNote);
    connect(scrap, &cwScrap::leadsReset, this, saveNote);

    // Transformations / type / view matrix
    connect(scrap, &cwScrap::noteTransformationChanged, this, saveNote);
    connect(scrap, &cwScrap::calculateNoteTransformChanged, this, saveNote);
    connect(scrap, &cwScrap::viewMatrixChanged, this, saveNote);
    connect(scrap, &cwScrap::typeChanged, this, saveNote);

    auto connectProjectedViewMatrixSignals = [this, saveNote, scrap]() {
        if (auto projected = qobject_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix()))
        {
            connect(projected, &cwProjectedProfileScrapViewMatrix::azimuthChanged, this, saveNote);
            connect(projected, &cwProjectedProfileScrapViewMatrix::directionChanged, this, saveNote);
        }
    };

    connectProjectedViewMatrixSignals();
    connect(scrap, &cwScrap::viewMatrixChanged, this, connectProjectedViewMatrixSignals);
}

void cwSaveLoad::connectNoteLiDAR(cwNoteLiDAR *lidarNote)
{
    if (lidarNote == nullptr) {
        return;
    }

    // Lambda to save this specific note
    const auto saveNote = [this, lidarNote]() {
        // qDebug() << "Save lidar:" << lidarNote << lidarNote->noteTransformation()->upSign();
        save(lidarNote);
    };

    if(!d->connectionChecker.add(lidarNote)) {
        return;
    }

    connect(lidarNote, &cwNoteLiDAR::filenameChanged, this, saveNote);
    connect(lidarNote, &cwNoteLiDAR::dataChanged,
            this, [saveNote](const QModelIndex &topLeft,
                       const QModelIndex &bottomRight,
                       const QList<int> &roles) {
        if(roles.contains(cwNoteLiDAR::NameRole)
            || roles.contains(cwNoteLiDAR::PositionOnNoteRole)) {
            Q_UNUSED(topLeft);
            Q_UNUSED(bottomRight);
            Q_UNUSED(roles);
            saveNote();
        }
    });
    connect(lidarNote, &cwNoteLiDAR::rowsInserted,
            this, [saveNote](const QModelIndex &parent, int first, int last) {
                Q_UNUSED(parent);
                Q_UNUSED(first);
                Q_UNUSED(last);
                saveNote();
    });
    connect(lidarNote, &cwNoteLiDAR::rowsRemoved,
            this, [saveNote](const QModelIndex &parent, int first, int last) {
                Q_UNUSED(parent);
                Q_UNUSED(first);
                Q_UNUSED(last);
                saveNote();
    });
    connect(lidarNote, &cwNoteLiDAR::autoCalculateNorthChanged, this, saveNote);

    if(!d->connectionChecker.add(lidarNote->noteTransformation())) {
        return;
    }

    connect(lidarNote->noteTransformation(), &cwNoteLiDARTransformation::upSignChanged, this, saveNote);
    connect(lidarNote->noteTransformation(), &cwNoteLiDARTransformation::upModeChanged, this, saveNote);
    connect(lidarNote->noteTransformation(), &cwNoteLiDARTransformation::upCustomChanged, this, saveNote);
    connect(lidarNote->noteTransformation(), &cwNoteLiDARTransformation::northUpChanged, this, saveNote);
    connect(lidarNote->noteTransformation(), &cwNoteLiDARTransformation::scaleChanged, this, saveNote);
}


// void cwSaveLoad::setFileNameHelper(const QString &fileName)
// {
//     if(d->projectFileName != fileName) {
//         d->projectFileName = fileName;

//         d->repository->setDirectory(QFileInfo(fileName).absoluteDir());
//         d->repository->initRepository();

//         emit fileNameChanged();
//     }
// }


void cwSaveLoad::setTemporary(bool isTemp)
{
    if(d->isTemporary != isTemp) {
        d->isTemporary = isTemp;
        emit isTemporaryProjectChanged();
    }
}

QString cwSaveLoad::randomName() const
{
    quint32 randomValue = QRandomGenerator::global()->generate();
    return QStringLiteral("cavewhereTmp-") + QStringLiteral("%1").arg(randomValue, 8, 16, QChar(u'0'));
}

QDir cwSaveLoad::createTemporaryDirectory(const QString &subDirName)
{
    QTemporaryDir tempDir;
    tempDir.setAutoRemove(false);

    QDir dir(tempDir.path());
    dir.mkdir(subDirName);
    dir.cd(subDirName);

    return dir;
}

QFuture<void> cwSaveLoad::completeSaveJobs()
{
    bool hasRunningJobs = !d->m_runningJobs.isEmpty();
    bool hasWaitingJobs = !d->m_waitingJobs.empty();

    if(hasRunningJobs || hasWaitingJobs) {
        auto combine = AsyncFuture::combine();

        if(hasRunningJobs) {
            for(auto keyValue : d->m_runningJobs.asKeyValueRange()) {
                combine << keyValue.second;
            }
        }

        if(hasWaitingJobs) {
            for(const auto& keyValue : d->m_waitingJobs) {
                combine << keyValue.second.jobDeferred.future();
            }
        }

        return combine.future();
    } else {
        return QtFuture::makeReadyVoidFuture();
    }
}

QString cwSaveLoad::sanitizeFileName(QString input) {
    // Modify the input string in-place
    const QString forbiddenChars = R"(\/:*?"<>|)";
    for (const QChar& ch : forbiddenChars) {
        input.replace(ch, "_");
    }

    input = input.trimmed();
    while (input.startsWith('.') || input.endsWith('.')) {
        input = input.mid(1).chopped(1);
    }

    if (input.isEmpty()) {
        input = "untitled";
    }

    return input;
}

QUuid cwSaveLoad::toUuid(const std::string &uuidStr)
{
    return QUuid::fromString(QString::fromStdString(uuidStr));
}

// QString cwSaveLoad::projectFileName(const cwProject *project)
// {
//     Q_ASSERT(false);
//     return QString();
// }

// QString cwSaveLoad::projectAbsolutePath(const cwProject *project)
// {
//     return QString(); //regionFileName();
// }


QStringList fromProtoStringList(const google::protobuf::RepeatedPtrField<std::string>& protoStringList)
{
    QStringList stringList;

    if(!protoStringList.empty()) {
        stringList.reserve(protoStringList.size());

        for (const auto& str : protoStringList) {
            stringList.append(QString::fromStdString(str));
        }
    }

    return stringList;
}

QDir cwSaveLoad::projectDir(const cwProject *project)
{
    QFileInfo info(project->filename());
    return info.absoluteDir();
}

QDir cwSaveLoad::projectDir() const
{
    return projectDir(d->m_regionTreeModel->cavingRegion()->parentProject());
}

QString cwSaveLoad::regionFileName(const cwCavingRegion *region)
{
    return sanitizeFileName(region->name() + QStringLiteral(".cw"));
}

QString cwSaveLoad::fileName(const cwCave *cave)
{
    return sanitizeFileName(cave->name() + QStringLiteral(".cwcave"));
}

QString cwSaveLoad::absolutePath(const cwCave *cave)
{
    return dir(cave).absoluteFilePath(fileName(cave));
}

QDir cwSaveLoad::dir(const cwCave *cave)
{
    if(cave->parentRegion() && cave->parentRegion()->parentProject()) {
        QDir projDir = projectDir(cave->parentRegion()->parentProject());
        return caveDirHelper(projDir, cave);
    } else {
        return QDir();
    }

}

QString cwSaveLoad::fileName(const cwTrip *trip)
{
    return sanitizeFileName(trip->name() + QStringLiteral(".cwtrip"));
}

QString cwSaveLoad::absolutePath(const cwTrip *trip)
{
    return dir(trip).absoluteFilePath(fileName(trip));
}

QDir cwSaveLoad::dir(const cwTrip *trip)
{
    if(trip->parentCave()) {
        return tripDirHelper(dir(trip->parentCave()), trip);
    } else {
        return QDir();
    }
}

QDir cwSaveLoad::dir(cwSurveyNoteModel *notes)
{
    if(notes->parentTrip()) {
        return noteDirHelper(dir(notes->parentTrip()));
    } else {
        return QDir();
    }
}

QDir cwSaveLoad::dir(cwSurveyNoteLiDARModel *notes)
{
    if(notes->parentTrip()) {
        return noteDirHelper(dir(notes->parentTrip()));
    } else {
        return QDir();
    }
}

QString cwSaveLoad::fileName(const cwNote *note)
{
    return sanitizeFileName(note->name() + QStringLiteral(".cwnote"));
}

cwImage cwSaveLoad::absolutePathNoteImage(const cwNote* note)
{
    if (!note) {
        return cwImage();
    }

    const QString path = absolutePath(note, note->image().path());
    if (path.isEmpty()) {
        return cwImage();
    }

    cwImage image = note->image();
    image.setPath(path);
    return image;
}

QString cwSaveLoad::absolutePath(const cwNote *note)
{
    return dir(note).absoluteFilePath(fileName(note));
}

QString cwSaveLoad::absolutePath(const cwNote *note, const QString& imageFilename)
{
    if(note == nullptr) {
        return QString();
    }

    if(!imageFilename.isEmpty()) {
        return dir(note).absoluteFilePath(imageFilename);
    }

    return QString();
}


QDir cwSaveLoad::dir(const cwNote *note)
{
    if(note->parentTrip()) {
        return noteDirHelper(dir(note->parentTrip()));
    } else {
        return QDir();
    }
}

QString cwSaveLoad::fileName(const cwNoteLiDAR *note)
{
    return sanitizeFileName(note->name() + QStringLiteral(".cwnote3d"));
}

QString cwSaveLoad::absolutePath(const cwNoteLiDAR *note)
{
    return dir(note).absoluteFilePath(fileName(note));
}

QString cwSaveLoad::absolutePath(const cwNoteLiDAR *note, const QString& lidarFilename)
{
    if(note == nullptr) {
        return QString();
    }

    if(lidarFilename.isEmpty()) {
        return QString();
    }

    return dir(note).absoluteFilePath(lidarFilename);
}

QDir cwSaveLoad::dir(const cwNoteLiDAR *note)
{
    if(note->parentTrip()) {
        return noteDirHelper(dir(note->parentTrip()));
    } else {
        return QDir();
    }
}

QFuture<ResultBase> cwSaveLoad::saveProtoMessage(const QDir &dir,
                                                 const QString &filename,
                                                 std::unique_ptr<const google::protobuf::Message> message,
                                                 const void* objectId)
{
    Q_ASSERT(message);
    return d->saveProtoMessage(this,
                               dir.absoluteFilePath(sanitizeFileName(filename)),
                               std::move(message),
                               objectId);

}

QDir cwSaveLoad::caveDirHelper(const QDir &projectDir, const cwCave *cave)
{
    QString caveDirName = sanitizeFileName(cave->name());
    return QDir(projectDir.absoluteFilePath(caveDirName));
}

QDir cwSaveLoad::tripDirHelper(const QDir &caveDir, const cwTrip *trip)
{
    return QDir(caveDir.absoluteFilePath(QStringLiteral("trips/") + sanitizeFileName(trip->name())));
}

QDir cwSaveLoad::noteDirHelper(const QDir &tripDir)
{
    return QDir(tripDir.absoluteFilePath("notes"));
}

QFuture<ResultBase> cwSaveLoad::Data::saveProtoMessage(
    cwSaveLoad* context,
    const QString &filename,
    std::unique_ptr<const google::protobuf::Message> message,
    const void* objectId)
{

    // qDebug () << "Try Saving to " << filename << objectId;

    //Rename or remove file and directories
    excuteFileSystemActions();

    //Make sure we're not saving to the same file at the same time
    if (m_runningJobs.contains(filename) || !m_fileSystemJobs.isEmpty()) {

        Q_ASSERT(!clearingWait);
        // qDebug() << "\twaiting runningJob:" << m_runningJobs.contains(filename) << "rename:" << !m_fileSystemJobs.isEmpty();

        auto deferred = AsyncFuture::Deferred<ResultBase>();

        m_waitingJobs[filename] = WaitingJob {
            deferred,
            std::move(message),
        };

        //Add defered here
        return deferred.future();
    } else {

        auto future = cwConcurrent::run([filename, message = std::move(message)]() {

            //enable this to simulate slow saves, good for debugging thread code
            // QThread::currentThread()->msleep(100);

            return mbind(ensurePathForFile(filename), [&](ResultBase /*result*/) {
                QSaveFile file(filename);
                if (!file.open(QFile::WriteOnly)) {
                    qWarning() << "Failed to write to " << filename << file.errorString();
                    return Monad::ResultBase(QStringLiteral("Failed to open file for writing: %1").arg(filename));
                }

                std::string json_output;
                google::protobuf::util::JsonPrintOptions options;
                options.add_whitespace = true; // Makes it pretty
                // options.always_print_primitive_fields = true; // Optional: print even if fields are default

                auto status = google::protobuf::util::MessageToJsonString(*message, &json_output, options);
                if (!status.ok()) {
                    return Monad::ResultBase(QStringLiteral("Failed to convert proto message to JSON: %1").arg(status.ToString().c_str()));
                }

                file.write(json_output.c_str(), json_output.size());
                file.commit();

                // qDebug() << "Saving:" << filename << "error code:" << file.error();

                return Monad::ResultBase();
            });
        });

        AsyncFuture::observe(future).context(
            context, [filename, this, context, objectId]() {
                // qDebug() << "job finished:" << filename << objectId;
                m_runningJobs.remove(filename);
                m_fileLookup[objectId] = filename;

                auto runWaitingJobs = [this, context, objectId]() {
                    Q_ASSERT(m_fileSystemJobs.isEmpty());
                    Q_ASSERT(m_runningJobs.isEmpty());

                    //clearingWait is a temp variable for debugging, TODO: remove
                    clearingWait = true;

                    // Run waiting save jobs
                    for (auto it = m_waitingJobs.begin(); it != m_waitingJobs.end(); ++it) {
                        WaitingJob& job = it->second;

                        //Recursive call
                        auto future = saveProtoMessage(context, it->first, std::move(job.message), objectId);
                        AsyncFuture::observe(future).context(context, [deferred = job.jobDeferred, future]() mutable {
                            deferred.complete(future.result());
                        });
                    }
                    m_waitingJobs.clear();

                    clearingWait = false;
                };

                if(m_runningJobs.isEmpty()) {
                    if(!m_fileSystemJobs.isEmpty()) {
                        excuteFileSystemActions();
                    }
                    runWaitingJobs();
                }
            });

        m_runningJobs.insert(filename, QFuture<void>(future));

        return future;
    }
}


bool cwSaveLoad::isTemporaryProject() const
{
    return d->isTemporary;
}
