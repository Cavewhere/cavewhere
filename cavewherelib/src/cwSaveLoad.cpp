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
#include "cwImageUtils.h"
#include "cwSvgReader.h"
#include "cwNoteLiDAR.h"
#include "cwImageResolution.h"
#include "cwUniqueConnectionChecker.h"
#include "cwGlobals.h"

//Async future
#include <asyncfuture.h>
#include <memory>

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
#include <algorithm>
#include <atomic>
#include <variant>
#include <type_traits>

using namespace Monad;

static QStringList fromProtoStringList(const google::protobuf::RepeatedPtrField<std::string> &protoStringList);

namespace {

QDir projectRootDirForFile(const QString& projectFileName)
{
    QFileInfo info(projectFileName);
    return info.absoluteDir();
}

QDir gitDirForRepository(const QDir& repoDir)
{
    const QString dotGitPath = repoDir.filePath(QStringLiteral(".git"));
    const QFileInfo gitInfo(dotGitPath);
    if (gitInfo.isDir()) {
        return QDir(gitInfo.absoluteFilePath());
    }

    if (!gitInfo.isFile()) {
        return QDir();
    }

    QFile gitFile(gitInfo.absoluteFilePath());
    if (!gitFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QDir();
    }

    const QByteArray line = gitFile.readLine();
    const QByteArray gitDirPrefix = "gitdir:";
    if (!line.startsWith(gitDirPrefix)) {
        return QDir();
    }

    QString gitDirPath = QString::fromUtf8(line.mid(gitDirPrefix.size())).trimmed();
    if (gitDirPath.isEmpty()) {
        return QDir();
    }

    QDir gitDir(gitDirPath);
    if (gitDir.isRelative()) {
        gitDir = QDir(repoDir.filePath(gitDirPath));
    }

    return gitDir.exists() ? gitDir : QDir();
}

QString defaultDataRoot(const QString& projectName)
{
    return cwSaveLoad::sanitizeFileName(projectName);
}

CavewhereProto::ProjectMetadata::GitMode toProtoGitMode(cwSaveLoad::GitMode mode)
{
    switch (mode) {
    case cwSaveLoad::GitMode::ManagedNew:
        return CavewhereProto::ProjectMetadata::GitMode::ProjectMetadata_GitMode_ManagedNew;
    case cwSaveLoad::GitMode::ExistingRepo:
        return CavewhereProto::ProjectMetadata::GitMode::ProjectMetadata_GitMode_ExistingRepo;
    case cwSaveLoad::GitMode::NoGit:
        return CavewhereProto::ProjectMetadata::GitMode::ProjectMetadata_GitMode_NoGit;
    }

    return CavewhereProto::ProjectMetadata::GitMode::ProjectMetadata_GitMode_ManagedNew;
}

cwSaveLoad::GitMode fromProtoGitMode(CavewhereProto::ProjectMetadata::GitMode mode)
{
    switch (mode) {
    case CavewhereProto::ProjectMetadata::GitMode::ProjectMetadata_GitMode_ExistingRepo:
        return cwSaveLoad::GitMode::ExistingRepo;
    case CavewhereProto::ProjectMetadata::GitMode::ProjectMetadata_GitMode_NoGit:
        return cwSaveLoad::GitMode::NoGit;
    case CavewhereProto::ProjectMetadata::GitMode::ProjectMetadata_GitMode_ManagedNew:
    default:
        return cwSaveLoad::GitMode::ManagedNew;
    }
}

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

namespace cw::git {

void ensureGitExcludeHasCacheEntry(const QDir& repoDir)
{
    const QDir gitDir = gitDirForRepository(repoDir);
    if (!gitDir.exists()) {
        return;
    }

    const QDir infoDir(gitDir.filePath(QStringLiteral("info")));
    if (!infoDir.exists()) {
        if (!QDir().mkpath(infoDir.absolutePath())) {
            return;
        }
    }

    const QString excludePath = infoDir.filePath(QStringLiteral("exclude"));
    QByteArray existingContents;
    if (QFile::exists(excludePath)) {
        QFile readFile(excludePath);
        if (readFile.open(QIODevice::ReadOnly)) {
            existingContents = readFile.readAll();
        }
    }

    const QString entry = QStringLiteral(".cw_cache/");
    const QStringList lines = QString::fromUtf8(existingContents).split('\n');
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed == entry || trimmed == QStringLiteral(".cw_cache")) {
            return;
        }
    }

    QFile writeFile(excludePath);
    if (!writeFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return;
    }

    if (!existingContents.isEmpty() && !existingContents.endsWith('\n')) {
        writeFile.write("\n");
    }
    writeFile.write(".cw_cache/\n");
}

} // namespace cw::git


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

    struct Job {
        enum class Kind { File, Directory, };
        enum class Action { Move, Remove, EnsureDir, WriteFile, CopyFile, Custom };

        struct EmptyPayload { };
        struct WriteFilePayload { std::shared_ptr<const google::protobuf::Message> message; };
        struct CopyFilePayload { QString sourcePath; };
        struct CustomPayload { std::function<Monad::ResultBase()> action; };
        using Payload = std::variant<EmptyPayload, WriteFilePayload, CopyFilePayload, CustomPayload>;

        const void* objectId = nullptr;
        Kind kind = Kind::File;
        Action action = Action::Move;
        std::function<void(const Monad::ResultBase&)> onDone;
        Payload payload = EmptyPayload{};

        QString oldPath;
        QString path;
        QString dataRoot;

        Job() = default;
        Job(const void* objectId, Kind kind, Action action)
            : objectId(objectId),
            kind(kind),
            action(action),
            payload(EmptyPayload{})
        {
        }

        Job(const void* objectId,
            Kind kind,
            Action action,
            std::shared_ptr<const google::protobuf::Message> message)
            : objectId(objectId),
            kind(kind),
            action(action),
            payload(WriteFilePayload{std::move(message)})
        {
        }

        Job(const void* objectId, Kind kind, Action action, QString sourcePath)
            : objectId(objectId),
            kind(kind),
            action(action),
            payload(CopyFilePayload{std::move(sourcePath)})
        {
        }

        Job(const void* objectId,
            Kind kind,
            Action action,
            std::function<Monad::ResultBase()> customAction)
            : objectId(objectId),
            kind(kind),
            action(action),
            payload(CustomPayload{std::move(customAction)})
        {
        }

        // ~Job() {
        //     qDebug() << "sizeOf:" << sizeof(Job);
        // }

        static const char* kindName(Kind kind) {
            switch (kind) {
            case Kind::File:
                return "File";
            case Kind::Directory:
                return "Directory";
            }
            return "Unknown";
        }

        static const char* actionName(Action action) {
            switch (action) {
            case Action::Move:
                return "Move";
            case Action::Remove:
                return "Remove";
            case Action::EnsureDir:
                return "EnsureDir";
            case Action::WriteFile:
                return "WriteFile";
            case Action::CopyFile:
                return "CopyFile";
            case Action::Custom:
                return "Custom";
            }
            return "Unknown";
        }

        QString toString() const {
            const QString objectHex = QString::number(reinterpret_cast<quintptr>(objectId), 16);
            const auto* writePayload = std::get_if<WriteFilePayload>(&payload);
            const auto* copyPayload = std::get_if<CopyFilePayload>(&payload);
            const auto* customPayload = std::get_if<CustomPayload>(&payload);
            const QString sourcePath = copyPayload ? copyPayload->sourcePath : QString();
            return QStringLiteral("Job{action=%1 kind=%2 objectId=0x%3 oldPath='%4' newPath='%5' sourcePath='%6' dataRoot='%7' hasMessage=%8 hasCustom=%9}")
                .arg(QString::fromLatin1(actionName(action)),
                     QString::fromLatin1(kindName(kind)),
                     objectHex,
                     oldPath,
                     path,
                     sourcePath,
                     dataRoot)
                .arg(writePayload && writePayload->message != nullptr)
                .arg(customPayload && customPayload->action != nullptr);
        }

        Monad::ResultBase execute() const {
            auto isInsideRoot = [](const QString& rootPath, const QString& targetPath) {
                if (rootPath.isEmpty() || targetPath.isEmpty()) {
                    return false;
                }
                const QString root = QDir::cleanPath(QDir(rootPath).absolutePath());
                if (root.isEmpty()) {
                    return false;
                }
                const QString target = QDir::cleanPath(QFileInfo(targetPath).absoluteFilePath());
                if (target.isEmpty()) {
                    return false;
                }
                return target == root || target.startsWith(root + QStringLiteral("/"));
            };

            auto isProjectFile = [&](const QString& targetPath) {
                const QString root = QDir::cleanPath(QDir(dataRoot).absolutePath());
                const QString target = QDir::cleanPath(QFileInfo(targetPath).absolutePath());
                const QString parentOfRoot = QDir(root).absolutePath() + QStringLiteral("/") + QStringLiteral("..");
                const QString normalizedParent = QDir::cleanPath(QDir(parentOfRoot).absolutePath());
                const bool isProjectFileWrite = (action == Action::WriteFile && kind == Kind::File);
                const bool isInProjectRoot = (target == normalizedParent)
                                             || target.startsWith(normalizedParent + QStringLiteral("/"));

                // qDebug() << "Root:" << root << target << parentOfRoot << normalizedParent << isProjectFileWrite << isInProjectRoot;

                return isProjectFileWrite && isInProjectRoot;
            };

            auto ensureInsideRoot = [&](const QString& targetPath) -> Monad::ResultBase {
                if (!isInsideRoot(dataRoot, targetPath) && !isProjectFile(targetPath)) [[unlikely]] {
                    return Monad::ResultBase(QStringLiteral("Path outside dataRoot: %1").arg(targetPath));
                }
                return Monad::ResultBase();
            };

            switch(action) {
            case Action::EnsureDir: {
                auto rootCheck = ensureInsideRoot(path);
                if (rootCheck.hasError()) {
                    return rootCheck;
                }
                QDir dir;
                if (!dir.mkpath(path)) {
                    return Monad::ResultBase(QStringLiteral("Failed to create directory: %1").arg(path));
                }
                return Monad::ResultBase();
            }
            case Action::Move:
            {
                auto oldCheck = ensureInsideRoot(oldPath);
                if (oldCheck.hasError()) {
                    return oldCheck;
                }
                auto newCheck = ensureInsideRoot(path);
                if (newCheck.hasError()) {
                    return newCheck;
                }
            }
                Q_ASSERT(QFileInfo::exists(oldPath));
                // Q_ASSERT(QFileInfo::exists(path));
                if (!QDir().rename(oldPath, path)) {
                    return Monad::ResultBase(QStringLiteral("Failed to move %1 -> %2").arg(oldPath, path));
                }
                Q_ASSERT(QFileInfo::exists(path));
                return Monad::ResultBase();
            case Action::Remove:
            {
                auto rootCheck = ensureInsideRoot(oldPath);
                if (rootCheck.hasError()) {
                    return rootCheck;
                }
            }
                switch(kind) {
                case Kind::File:
                    if (!QFileInfo::exists(oldPath)) {
                        return Monad::ResultBase();
                    }
                    Q_ASSERT(QFileInfo(oldPath).isFile());
                    if (!QFile::remove(oldPath)) {
                        return Monad::ResultBase(QStringLiteral("Failed to remove file: %1").arg(oldPath));
                    }
                    return Monad::ResultBase();
                case Kind::Directory:
                    if (!QFileInfo::exists(oldPath)) {
                        return Monad::ResultBase();
                    }
                    Q_ASSERT(QFileInfo(oldPath).isDir());
                    if (!QDir(oldPath).removeRecursively()) {
                        return Monad::ResultBase(QStringLiteral("Failed to remove directory: %1").arg(oldPath));
                    }
                    return Monad::ResultBase();
                }
            case Action::WriteFile: {
                auto* writePayload = std::get_if<WriteFilePayload>(&payload);
                if (!writePayload || !writePayload->message) {
                    return Monad::ResultBase(QStringLiteral("Missing message for WriteFile job: %1").arg(path));
                }
                {
                    auto rootCheck = ensureInsideRoot(path);
                    if (rootCheck.hasError()) {
                        return rootCheck;
                    }
                }
                return mbind(ensurePathForFile(path), [&](ResultBase /*result*/) {
                    QSaveFile file(path);
                    if (!file.open(QFile::WriteOnly)) {
                        qWarning() << "Failed to write to " << path << file.errorString();
                        return Monad::ResultBase(QStringLiteral("Failed to open file for writing: %1").arg(path));
                    }

                    std::string json_output;
                    google::protobuf::util::JsonPrintOptions options;
                    options.add_whitespace = true;

                    auto status = google::protobuf::util::MessageToJsonString(*writePayload->message, &json_output, options);
                    if (!status.ok()) {
                        return Monad::ResultBase(QStringLiteral("Failed to convert proto message to JSON: %1").arg(status.ToString().c_str()));
                    }

                    file.write(json_output.c_str(), json_output.size());
                    file.commit();
                    return Monad::ResultBase();
                });
            }
            case Action::CopyFile: {
                auto* copyPayload = std::get_if<CopyFilePayload>(&payload);
                if (!copyPayload || copyPayload->sourcePath.isEmpty()) {
                    return Monad::ResultBase(QStringLiteral("Missing source path for CopyFile job: %1").arg(path));
                }
                {
                    auto rootCheck = ensureInsideRoot(path);
                    if (rootCheck.hasError()) {
                        return rootCheck;
                    }
                }
                return mbind(ensurePathForFile(path), [&](ResultBase /*result*/) {
                    // QThread::msleep(10000);
                    // qDebug() << "Coping sourcePath:" << copyPayload->sourcePath << path;
                    if (!QFile::copy(copyPayload->sourcePath, path)) {
                        return Monad::ResultBase(QStringLiteral("Failed to copy %1 -> %2").arg(copyPayload->sourcePath, path));
                    }
                    return Monad::ResultBase();
                });
            }
            case Action::Custom:
                if (auto* customPayload = std::get_if<CustomPayload>(&payload)) {
                    if (!customPayload->action) {
                        return Monad::ResultBase(QStringLiteral("Missing custom action for job"));
                    }
                    return customPayload->action();
                }
                return Monad::ResultBase(QStringLiteral("Missing custom action for job"));
            }
            return Monad::ResultBase();
        }

        static bool lessThan(const Job& a, const Job& b) {
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

            auto kindRank = [](Job::Kind kind) -> int {
                // Files first (0), directories second (1).
                return (kind == Job::Kind::File) ? 0 : 1;
            };

            auto actionRank = [](Job::Action action) -> int {
                return action == Job::Action::Remove ? 0 : 1;
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

            const QString aPath = a.action == Action::EnsureDir ? a.path : a.oldPath;
            const QString bPath = b.action == Action::EnsureDir ? b.path : b.oldPath;
            const int aDepth = pathDepth(aPath);
            const int bDepth = pathDepth(bPath);
            if (aDepth != bDepth) {
                // Deeper first.
                return aDepth > bDepth;
            }

            // Stable lexical tie-breaker.
            return aPath < bPath;
        }
    };

    QQuickGit::GitRepository* repository;

    QString projectFileName;
    cwSaveLoad::ProjectMetadataData projectMetadata;

    struct ObjectState {
        QString currentPath;
    };

    //Where the objects are currently being saved
    //This the absolute directory to the m_rootDir
    QHash<const void*, ObjectState> m_objectStates;

    //For watching when object data has changed
    cwRegionTreeModel* m_regionTreeModel;

    //Saving jobs
    QList<Job> m_pendingJobs;
    AsyncFuture::Deferred<void> m_pendingJobsDeferred;

    bool isTemporary = true;
    bool saveEnabled = true;
    bool newProjectCalled = false;

    cwFutureManagerToken futureToken;

    //Helps watch if we already has objects connected, this is for debugging only
    cwUniqueConnectionChecker connectionChecker;

    bool retiring = false;
    QFuture<void> retireFuture;

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

    Data() {
        m_pendingJobsDeferred.complete();
    }

    void addFileSystemJob(Job job, cwSaveLoad* context) {
        Q_ASSERT(job.path.isEmpty());
        Q_ASSERT(job.oldPath.isEmpty());

        const QString dataRootName = context->dataRoot();
        const QString dataRootPath = dataRootName.isEmpty()
                                         ? context->projectRootDir().absolutePath()
                                         : context->projectRootDir().absoluteFilePath(dataRootName);
        job.dataRoot = QDir(dataRootPath).absolutePath();

        // if(job.kind == Job::Kind::Directory) {
        //     qDebug() << "Directory move!";
        // }

        auto toDirOrFilePath = [](const Job& job, const QString& path) {
            return job.kind == Job::Kind::Directory ? QFileInfo(path).absoluteDir().absolutePath() : path;
        };

        auto oldPath = [this, toDirOrFilePath](const Job& job) {
            return toDirOrFilePath(job, m_objectStates[job.objectId].currentPath);
        };

        auto path = [this, context, toDirOrFilePath](const Job& job) {
            return toDirOrFilePath(job, absolutePathFor(context, static_cast<const QObject*>(job.objectId)));
        };

        job.path = path(job);
        job.oldPath = oldPath(job);


        auto emitObjectPathHelper = [](const Monad::ResultBase& result, auto doneFunc, auto emitFunc) {
            if (doneFunc) {
                doneFunc(result);
            }

            if (!result.hasError()) {
                emitFunc();
            }
        };

        if (job.action == Job::Action::WriteFile && job.kind == Job::Kind::File) {
            auto& state = m_objectStates[job.objectId];
            if (state.currentPath.isEmpty()) {
                state.currentPath = job.path;
            } else {
                // qDebug() << "Paths:" << state.currentPath << job.path;
                Q_ASSERT(state.currentPath == job.path);
            }
        } else if (job.kind == Job::Kind::File && job.action == Job::Action::Move) {
            auto& state = m_objectStates[job.objectId];
            Q_ASSERT(state.currentPath != job.path);
            state.currentPath = job.path;

            const auto originalOnDone = job.onDone;
            job.onDone =
                [this,
                 emitObjectPathHelper,
                 context,
                 objectId = job.objectId,
                 originalOnDone]
                (const Monad::ResultBase& result)
            {
                emitObjectPathHelper(result, originalOnDone, [objectId, context]() {
                    if (auto* object = static_cast<QObject*>(const_cast<void*>(objectId))) {
                        emit context->objectPathReady(object);
                    }
                });
            };
        } else if (job.kind == Job::Kind::Directory && job.action == Job::Action::Move) {
            const QString oldDir = job.oldPath;
            const QString newDir = job.path;

            Q_ASSERT(!oldDir.isEmpty());
            Q_ASSERT(!newDir.isEmpty());
            Q_ASSERT(oldDir != newDir);

            const QString prefix = oldDir + QStringLiteral("/");
            for (auto it = m_objectStates.begin(); it != m_objectStates.end(); ++it) {
                const QString currentPath = it.value().currentPath;
                if (currentPath == oldDir) {
                    it.value().currentPath = newDir;
                } else if (currentPath.startsWith(prefix)) {
                    it.value().currentPath = newDir + QStringLiteral("/") + currentPath.mid(prefix.size());
                }
            }

            const auto originalOnDone = job.onDone;
            job.onDone =
                [this,
                 emitObjectPathHelper,
                 context,
                 objectId = job.objectId,
                 originalOnDone]
                (const Monad::ResultBase& result)
            {
                emitObjectPathHelper(result, originalOnDone, [objectId, context]() {
                    // Emit only for the directory object; listeners cascade so we avoid N per-file signals.
                    if (auto* object = static_cast<QObject*>(const_cast<void*>(objectId))) {
                        emit context->objectPathReady(object);
                    }
                });
            };
        } else if (job.kind == Job::Kind::Directory && job.action == Job::Action::Remove) {
            const QString oldDir = job.oldPath;
            if (!oldDir.isEmpty()) {
                const QString prefix = oldDir + QStringLiteral("/");
                for (auto it = m_objectStates.begin(); it != m_objectStates.end();) {
                    const QString currentPath = it.value().currentPath;
                    if (currentPath == oldDir || currentPath.startsWith(prefix)) {
                        it = m_objectStates.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }

        m_pendingJobs.append(job);

        // qDebug() << "Pushing job:" << this << m_pendingJobs.size() << job.toString();

        if(m_pendingJobsDeferred.future().isFinished()) {
            // qDebug() << "Exec file system jobs:" << this << m_pendingJobs.size();
            execFileSystemJobs(context);
        }
    }

    void addExplicitFileSystemJob(Job job, cwSaveLoad* context) {
        // Explicit jobs are already path-resolved and don't track object state; move jobs here
        // can race with path updates and break path-ready notifications. Use addFileSystemJob for moves.
        Q_ASSERT(job.action != Job::Action::Move);

        const QString dataRootName = context->dataRoot();
        const QString dataRootPath = dataRootName.isEmpty()
                                         ? context->projectRootDir().absolutePath()
                                         : context->projectRootDir().absoluteFilePath(dataRootName);
        job.dataRoot = QDir(dataRootPath).absolutePath();

        m_pendingJobs.append(job);

        // qDebug() << "Pushing explicit job:" << this << m_pendingJobs.size() << job.toString();

        if(m_pendingJobsDeferred.future().isFinished()) {
            execFileSystemJobs(context);
        }
    }

    QString absolutePathFor(const cwSaveLoad* context, const QObject* object) const {
        // qDebug() << "Object:" << object;

        if (auto note = qobject_cast<const cwNote*>(object)) {
            return context->absolutePathPrivate(note);
        }
        if (auto lidar = qobject_cast<const cwNoteLiDAR*>(object)) {
            return context->absolutePathPrivate(lidar);
        }
        if (auto trip = qobject_cast<const cwTrip*>(object)) {
            return context->absolutePathPrivate(trip);
        }
        if (auto cave = qobject_cast<const cwCave*>(object)) {
            return context->absolutePathPrivate(cave);
        }
        if(auto region = qobject_cast<const cwCavingRegion*>(object)) {
            return projectFileName;
        }
        return QString();
    }

    ObjectState& stateFor(const void* object) {
        return m_objectStates[object];
    }

    void resetObjectStates(cwSaveLoad* context) {
        m_objectStates.clear();

        auto addObjects = [this, context](auto objects) {
            for(const auto object : objects) {
                auto& state = stateFor(object);
                state.currentPath = absolutePathFor(context, object);
            }
        };

        addObjects(m_regionTreeModel->all<cwCave*>(QModelIndex(), &cwRegionTreeModel::cave));
        addObjects(m_regionTreeModel->all<cwTrip*>(QModelIndex(), &cwRegionTreeModel::trip));
        addObjects(m_regionTreeModel->all<cwNote*>(QModelIndex(), &cwRegionTreeModel::note));
    }


    void saveProtoMessage(
        cwSaveLoad* context,
        std::unique_ptr<const google::protobuf::Message> message,
        const void* objectId
        );

    void execFileSystemJobs(cwSaveLoad* context) {
        if (m_pendingJobs.isEmpty()) {
            return;
        }

        Job job = m_pendingJobs.takeFirst();


        if(m_pendingJobsDeferred.future().isFinished()) {
            m_pendingJobsDeferred = {};
        }

        Q_ASSERT(m_pendingJobsDeferred.future().isRunning());

        // qDebug() << "Starting job:" << this << job.toString();
        auto future = cwConcurrent::run([job]() {
            // qDebug() << "\tExecuting job:" << job.toString();
            return job.execute();
        });

        AsyncFuture::observe(future).context(context, [this, context, job, future]() {
            const auto data = future.result();
            if(data.hasError()) {
                qWarning() << "Save job error:" << data.errorMessage() << job.toString();
            }
            if (job.onDone) {
                job.onDone(data);
            }
            if (m_pendingJobs.isEmpty()) {
                // qDebug() << "Pending jobs complete!" << this;
                m_pendingJobsDeferred.complete();
            } else {
                //Start another job
                execFileSystemJobs(context);
            }
        });
    }

    template<typename T>
    void saveObject(cwSaveLoad* context, const T* object) {
        if(saveEnabled) {
            // qDebug() << "Saving object:" << object << object->name() << dir(object);
            if constexpr (std::is_same_v<T, cwCave>) {
                saveProtoMessage(context, cwSaveLoad::toProtoCave(object), object);
            } else if constexpr (std::is_same_v<T, cwTrip>) {
                saveProtoMessage(context, cwSaveLoad::toProtoTrip(object), object);
            } else if constexpr (std::is_same_v<T, cwNote>) {
                saveProtoMessage(context, cwSaveLoad::toProtoNote(object), object);
            } else if constexpr (std::is_same_v<T, cwNoteLiDAR>) {
                saveProtoMessage(context, cwSaveLoad::toProtoNoteLiDAR(object), object);
            } else {
                static_assert(std::is_same_v<T, void>, "Unsupported saveObject type");
            }
        }
    }

    template<typename T>
    void renameDirectoryAndFile(cwSaveLoad* context, const T* object) {
        // Capture paths now to avoid partial state during rename.
        auto& state = stateFor(object);
        const QString oldFilePath = state.currentPath;
        QString newDirPath;
        if constexpr (std::is_same_v<T, cwCave>) {
            newDirPath = context->dirPrivate(object).absolutePath();
        } else if constexpr (std::is_same_v<T, cwTrip>) {
            newDirPath = context->dirPrivate(object).absolutePath();
        } else if constexpr (std::is_same_v<T, cwNote>) {
            newDirPath = context->dirPrivate(object).absolutePath();
        } else if constexpr (std::is_same_v<T, cwNoteLiDAR>) {
            newDirPath = context->dirPrivate(object).absolutePath();
        } else {
            static_assert(std::is_same_v<T, void>, "Unsupported renameDirectoryAndFile type");
        }

        Q_UNUSED(oldFilePath);
        Q_UNUSED(newDirPath);

        // qDebug() << "Rename" << oldFilePath << newDirPath;
        addFileSystemJob(Data::Job {object, Data::Job::Kind::Directory, Data::Job::Action::Move}, context);
        addFileSystemJob(Data::Job {object, Data::Job::Kind::File, Data::Job::Action::Move}, context);
        context->save(object);
    }
};

// cwSaveLoad::~cwSaveLoad() = default;
cwSaveLoad::~cwSaveLoad() {
    // qDebug() << "SaveLoad destroyed:" << this << d->m_pendingJobs.size();
    Q_ASSERT(d->m_pendingJobs.isEmpty());
    Q_ASSERT(d->m_pendingJobsDeferred.future().isFinished());
}

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

    localPath = cwGlobals::addExtension(localPath, QStringLiteral("cwproj"));
    if (QFileInfo(localPath).suffix().compare(QStringLiteral("cw"), Qt::CaseInsensitive) == 0) {
        localPath = QFileInfo(localPath).path() + QDir::separator()
        + QFileInfo(localPath).completeBaseName() + QStringLiteral(".cwproj");
    }
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
    const QString projectFilePath = rootDir.absoluteFilePath(sanitizedBaseName + QStringLiteral(".cwproj"));

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

    const QString currentRootPath = projectRootDir().absolutePath();
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
    const QString desiredFilePath = destination.projectFilePath;
    const QString sourceFileName = QFileInfo(currentProjectFile).fileName();
    QString sourceFilePath = targetRootDir.absoluteFilePath(sourceFileName);

    if (!QFileInfo::exists(sourceFilePath)) {
        const QString label = mode == ProjectTransferMode::Move ? QStringLiteral("Moved") : QStringLiteral("Copied");
        return ResultBase(QStringLiteral("%1 project file '%2' is missing.").arg(label, sourceFilePath));
    }

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

    const QString newDataRootName = destination.sanitizedBaseName;
    const auto region = d->m_regionTreeModel->cavingRegion();
    QString oldDataRootName = d->projectMetadata.dataRoot;
    if (oldDataRootName.isEmpty()) {
        oldDataRootName = defaultDataRoot(region ? region->name() : QString());
    }

    if (!oldDataRootName.isEmpty() && oldDataRootName != newDataRootName) {
        const QString oldDataRootPath = targetRootDir.absoluteFilePath(oldDataRootName);
        const QString newDataRootPath = targetRootDir.absoluteFilePath(newDataRootName);
        if (QFileInfo::exists(oldDataRootPath)) {
            if (QFileInfo::exists(newDataRootPath)) {
                return ResultBase(QStringLiteral("Destination data root '%1' already exists.").arg(newDataRootPath));
            }
            if (!QDir().rename(oldDataRootPath, newDataRootPath)) {
                return ResultBase(QStringLiteral("Couldn't rename data root to '%1'.").arg(newDataRootPath));
            }
        } else {
            QDir(targetRootDir).mkpath(newDataRootName);
        }
    }

    setFileName(desiredFilePath);
    setTemporary(false);
    d->projectMetadata.dataRoot = newDataRootName;
    emit dataRootChanged();
    if (region != nullptr) {
        region->setName(newDataRootName);
    }
    d->resetObjectStates(this);
    saveProject(targetRootDir, region);

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
    Q_ASSERT(!d->newProjectCalled);
    d->newProjectCalled = true;

    Q_ASSERT(d->m_pendingJobs.isEmpty());

    //Disconnect all connections
    disconnectTreeModel();

    //Clear the current data
    auto region = d->m_regionTreeModel->cavingRegion();

    if(region) [[likely]] {
        //Rename the region
        const auto tempName = randomName();
        region->setName(tempName);

        //Create the temp directory
        auto tempDir = createTemporaryDirectory(tempName);

        //Save the project file
        d->projectMetadata.dataRoot = defaultDataRoot(region->name());
        d->projectMetadata.gitMode = GitMode::ManagedNew;
        d->projectMetadata.syncEnabled = true;
        tempDir.mkpath(d->projectMetadata.dataRoot);

        setTemporary(true);
        setFileName(regionFileName(tempDir, region));

        saveProject(tempDir, region);

        //Connect all for watching for saves
        connectTreeModel();

        emit dataRootChanged();
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
                                            auto projectDataFuture = cwSaveLoad::loadAll(filename);

                                            d->futureToken.addJob({QFuture<void>(projectDataFuture), QStringLiteral("Loading")});

                                            return AsyncFuture::observe(projectDataFuture)
                                                .context(this, [this, projectDataFuture, filename]() {
                                                    return mbind(projectDataFuture, [this, projectDataFuture, filename](const ResultBase&) {
                                                        // setTemporaryProject(false);
                                                        //The filename needs to be set first because, image providers should
                                                        //have the filename before the region model is set
                                                        setFileName(filename);
                                                        setTemporary(false);

                                                        setSaveEnabled(false);
                                                        const auto& loadData = projectDataFuture.result().value();
                                                        d->projectMetadata = loadData.metadata;
                                                        emit dataRootChanged();
                                                        d->m_regionTreeModel->cavingRegion()->setData(loadData.region);

                                                        // d->projectFileName = filename;

                                                        d->resetObjectStates(this);

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
            cw::git::ensureGitExcludeHasCacheEntry(d->repository->directory());
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

void cwSaveLoad::saveProject(const QDir &dir, const cwCavingRegion *region)
{
    Q_UNUSED(dir);
    saveProtoMessage(toProtoProject(region), region);
}

std::unique_ptr<CavewhereProto::Project> cwSaveLoad::toProtoProject(const cwCavingRegion *region)
{
    auto protoProject = std::make_unique<CavewhereProto::Project>();
    auto fileVersion = protoProject->mutable_fileversion();
    fileVersion->set_version(cwRegionIOTask::protoVersion());
    cwRegionSaveTask::saveString(fileVersion->mutable_cavewhereversion(), CavewhereVersion);

    if (region != nullptr) {
        cwRegionSaveTask::saveString(protoProject->mutable_name(), region->name());
    }

    auto metadata = d->projectMetadata;
    if (metadata.dataRoot.isEmpty()) {
        metadata.dataRoot = defaultDataRoot(region ? region->name() : QString());
    }
    d->projectMetadata = metadata;

    auto protoMetadata = protoProject->mutable_metadata();
    cwRegionSaveTask::saveString(protoMetadata->mutable_dataroot(), metadata.dataRoot);
    protoMetadata->set_gitmode(toProtoGitMode(metadata.gitMode));
    protoMetadata->set_syncenabled(metadata.syncEnabled);

    return protoProject;
}

void cwSaveLoad::saveCavingRegion(const cwCavingRegion *region)
{
    saveProtoMessage(toProtoCavingRegion(region), region);
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
    return dir.absoluteFilePath(sanitizeFileName(QStringLiteral("%1.cwproj").arg(region->name())));
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

std::unique_ptr<CavewhereProto::Cave> cwSaveLoad::toProtoCave(const cwCave *cave)
{
    auto protoCave = std::make_unique<CavewhereProto::Cave>();
    auto fileVersion = protoCave->mutable_fileversion();
    fileVersion->set_version(cwRegionIOTask::protoVersion());
    cwRegionSaveTask::saveString(fileVersion->mutable_cavewhereversion(), CavewhereVersion);
    *(protoCave->mutable_name()) = cave->name().toStdString();
    return protoCave;
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
    int index = 0;
    QString sourceFilePath;
    QString destinationFilePath;
};

static QString uniqueDestinationPath(const QDir& destinationDirectory,
                                     const QString& sourceFilePath,
                                     const QSet<QString>& reservedPaths)
{
    const QFileInfo sourceInfo(sourceFilePath);
    const QString baseName = sourceInfo.completeBaseName();
    const QString suffix = sourceInfo.suffix();

    auto isTaken = [&reservedPaths](const QString& path) {
        return QFileInfo::exists(path) || reservedPaths.contains(path);
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
    const QDir rootDirectory = dataRootDir();
    Q_ASSERT(rootDirectory.exists());
    // qDebug() << "RootDir:" << rootDirectory;

    QList<CopyCommand> commands;
    commands.reserve(sourceFilePaths.size());
    QSet<QString> reservedPaths;
    reservedPaths.reserve(sourceFilePaths.size());
    int index = 0;
    for (const QString& source : sourceFilePaths) {
        const QString destination = uniqueDestinationPath(destinationDirectory,
                                                          source,
                                                          reservedPaths);
        commands.append(CopyCommand{index, source, destination});
        reservedPaths.insert(destination);
        ++index;
    }

    if (commands.isEmpty()) {
        outputCallBackFunc({});
        return;
    }

    struct CopyBatchState {
        QVector<Monad::Result<QString>> results;
        std::atomic<int> remaining{0};
        AsyncFuture::Deferred<void> deferred;

        explicit CopyBatchState(int count)
            : results(count),
            remaining(count)
        {
        }
    };

    auto state = std::make_shared<CopyBatchState>(commands.size());

    auto finalizeResults = [state, makeResult, outputCallBackFunc]() {
        QList<ResultType> finalResults;
        finalResults.reserve(state->results.size());

        std::transform(state->results.begin(), state->results.end(),
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
        state->deferred.complete();
    };

    for (const CopyCommand& command : commands) {
        Data::Job job;
        job.action = Data::Job::Action::CopyFile;
        job.kind = Data::Job::Kind::File;
        job.payload = Data::Job::CopyFilePayload{command.sourceFilePath};
        job.path = command.destinationFilePath;
        job.onDone = [state, command, rootDirectory, finalizeResults](const Monad::ResultBase& result) {
            if (!result.hasError()) {
                state->results[command.index] = Monad::ResultString(rootDirectory.relativeFilePath(command.destinationFilePath));
            } else {
                state->results[command.index] = Monad::ResultString(result.errorMessage(), result.errorCode());
            }

            if (state->remaining.fetch_sub(1) == 1) {
                finalizeResults();
            }
        };

        d->addExplicitFileSystemJob(job, this);
    }

    d->futureToken.addJob(cwFuture(QFuture<void>(state->deferred.future()),
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
        const QDir rootDirectory = dataRootDir();
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
        const QDir rootDirectory = dataRootDir();

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

    QDir dir = dataRootDir();
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
    const QDir rootDirectory = dataRootDir();
    copyFilesAndEmitResults<QString>(
        sourceFilePaths,
        dir,
        makeRelativePathEcho(),
        fileCallBackFunc
        );
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

std::unique_ptr<CavewhereProto::NoteLiDAR> cwSaveLoad::toProtoNoteLiDAR(const cwNoteLiDAR *note)
{
    //Copy trip data into proto, on the main thread
    auto protoNote = std::make_unique<CavewhereProto::NoteLiDAR>();
    auto fileVersion = protoNote->mutable_fileversion();
    fileVersion->set_version(cwRegionIOTask::protoVersion());
    cwRegionSaveTask::saveString(fileVersion->mutable_cavewhereversion(), CavewhereVersion);

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

    const QString projectName = QFileInfo(project->filename()).baseName();
    d->projectMetadata.dataRoot = defaultDataRoot(projectName);
    d->projectMetadata.gitMode = GitMode::ManagedNew;
    d->projectMetadata.syncEnabled = true;
    emit dataRootChanged();

    const QDir dataRootDir = makeDir(QDir(dir.absoluteFilePath(d->projectMetadata.dataRoot)));

    auto saveNotes = [makeDir, this, projectFileName, dataRootDir](const QDir& tripDir, const cwSurveyNoteModel* notes) {
        const QDir noteDir = makeDir(noteDirHelper(tripDir));

        int imageIndex = 1;
        for(cwNote* note : notes->notes()) {

            auto noteData = note->data();
            QPointer<cwNote> notePtr(note);
            auto updatedNoteData = std::make_shared<cwNoteData>();

            // AsyncFuture::Deferred<ResultBase> noteDeferred;

            Data::Job saveImageJob;
            saveImageJob.objectId = note;
            saveImageJob.kind = Data::Job::Kind::File;
            saveImageJob.action = Data::Job::Action::Custom;
            saveImageJob.payload = Data::Job::CustomPayload{[projectFileName, dataRootDir, noteData, imageIndex, noteDir, updatedNoteData]() {
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
                bool success = file.open(QSaveFile::WriteOnly);
                file.write(imageData.data());
                file.commit();

                // qDebug() << "Saving image:" << success << filename << file.errorString();

                cwImage noteImage = noteCopy.image();
                QString relativeFilename = dataRootDir.relativeFilePath(filename);
                noteImage.setPath(relativeFilename);
                noteCopy.setImage(noteImage);

                *updatedNoteData = noteCopy.data();
                return ResultBase();
            }};

            saveImageJob.onDone = [this, notePtr, updatedNoteData](const ResultBase& result) mutable {
                notePtr->setData(*updatedNoteData);
                save(notePtr);
            };

            d->addExplicitFileSystemJob(saveImageJob, this);

            ++imageIndex;
        }
    };

    auto saveTrips = [this, projectFileName, makeDir, saveNotes](const QDir& caveDir, const cwCave* cave) {
        for(const auto trip : cave->trips()) {
            const QDir tripDir = makeDir(tripDirHelper(caveDir, trip));
            save(trip);
            saveNotes(tripDir, trip->notes());
        }
    };


    //Save the region's data
    cwCavingRegion region;
    region.setName(projectName);
    makeDir(dir);

    QString newProjectFilename = regionFileName(dir, &region);
    setFileName(newProjectFilename);

    saveProject(dir, &region);


    //Go through all the caves
    for(const auto cave : project->cavingRegion()->caves()) {
        const QDir caveDir = makeDir(caveDirHelper(dataRootDir, cave));
        save(cave);
        saveTrips(caveDir, cave);
    }

    return AsyncFuture::observe(d->m_pendingJobsDeferred.future())
        .context(this, [this, newProjectFilename]() {
            // qDebug() << "Finished" << this;
            return ResultString(newProjectFilename);
        })
        .future();
}

QFuture<Monad::Result<cwSaveLoad::ProjectLoadData>> cwSaveLoad::loadAll(const QString &filename)
{
    return cwConcurrent::run([filename]() {
        auto projectResult = loadProject(filename);

        return projectResult.then([filename](Result<ProjectLoadData> result) {
            ProjectLoadData loadData = result.value();

            QDir projectRootDir = projectRootDirForFile(filename);
            QDir regionDir = QDir(projectRootDir.absoluteFilePath(loadData.metadata.dataRoot));

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

                loadData.region.caves.append(cave);
            }

            return Result(loadData);
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

Monad::Result<cwSaveLoad::ProjectLoadData> cwSaveLoad::loadProject(const QString &filename)
{
    auto projectResult = loadMessage<CavewhereProto::Project>(filename);
    return Monad::mbind(projectResult, [](const Result<CavewhereProto::Project>& result)
                        {
                            auto projectProto = result.value();

                            if (!projectProto.has_fileversion() || !projectProto.fileversion().has_version()) {
                                return Result<ProjectLoadData>(QStringLiteral("Project file missing version information."));
                            }

                            const int fileVersion = projectProto.fileversion().version();
                            if (fileVersion < 8) {
                                return Result<ProjectLoadData>(QStringLiteral("Legacy CaveWhere v7 project files are not supported."));
                            }

                            if (fileVersion > cwRegionIOTask::protoVersion()) {
                                return Result<ProjectLoadData>(
                                    QStringLiteral("Project file version %1 is newer than supported version %2.")
                                        .arg(fileVersion)
                                        .arg(cwRegionIOTask::protoVersion()));
                            }

                            ProjectLoadData loadData;
                            if (projectProto.has_name()) {
                                loadData.region.name = QString::fromStdString(projectProto.name());
                            }

                            if (projectProto.has_metadata()) {
                                const auto& metadataProto = projectProto.metadata();
                                if (metadataProto.has_dataroot()) {
                                    loadData.metadata.dataRoot = QString::fromStdString(metadataProto.dataroot());
                                }
                                if (metadataProto.has_gitmode()) {
                                    loadData.metadata.gitMode = fromProtoGitMode(metadataProto.gitmode());
                                }
                                if (metadataProto.has_syncenabled()) {
                                    loadData.metadata.syncEnabled = metadataProto.syncenabled();
                                }
                            }

                            if (loadData.metadata.dataRoot.isEmpty()) {
                                loadData.metadata.dataRoot = defaultDataRoot(loadData.region.name);
                            }

                            return Result(loadData);
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
                    d->addFileSystemJob(Data::Job
                                        {
                                            object,
                                            Data::Job::Kind::Directory,
                                            Data::Job::Action::Remove
                                        },
                                        this);
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
                        auto noteFilename = absolutePathPrivate(note);
                        // qDebug() << "Deleting:" << noteFilename << note << note->name();
                        d->addFileSystemJob(Data::Job
                                            {
                                                note,
                                                Data::Job::Kind::File,
                                                Data::Job::Action::Remove
                                            },
                                            this);
                        break;
                    }
                    default:
                        break;
                    }

                    d->connectionChecker.remove(object);
                    disconnect(object, nullptr, this, nullptr);

                }
            });

    connectObjects();
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

        connect(chunk, &cwSurveyChunk::calibrationsChanged, this, saveTrip);

        connect(chunk, &cwSurveyChunk::dataChanged, this, saveTrip);
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
        auto fileMove = Data::Job {note, Data::Job::Kind::File, Data::Job::Action::Move};
        d->addFileSystemJob(fileMove, this);

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

    connect(note, &cwNote::insertedScraps, this, saveNote);
    connect(note, &cwNote::removedScraps, this, saveNote);
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
    return d->m_pendingJobsDeferred.future();
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

QDir cwSaveLoad::projectRootDir() const
{
    return projectRootDirForFile(d->projectFileName);
}

QDir cwSaveLoad::dataRootDir() const
{
    auto region = d->m_regionTreeModel->cavingRegion();
    QString dataRootName = d->projectMetadata.dataRoot;
    if (dataRootName.isEmpty()) {
        dataRootName = defaultDataRoot(region ? region->name() : QString());
    }
    return QDir(projectRootDir().absoluteFilePath(dataRootName));
}

QDir cwSaveLoad::dir(cwSurveyNoteModel* notes) const
{
    return dirPrivate(notes);
}

QDir cwSaveLoad::dir(cwSurveyNoteLiDARModel* notes) const
{
    return dirPrivate(notes);
}

QString cwSaveLoad::absolutePath(const cwNote* note, const QString& imageFilename) const
{
    return absolutePathPrivate(note, imageFilename);
}

QString cwSaveLoad::absolutePath(const cwNoteLiDAR* note, const QString& lidarFilename) const
{
    return absolutePathPrivate(note, lidarFilename);
}

cwImage cwSaveLoad::absolutePathNoteImage(const cwNote* note) const
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

QString cwSaveLoad::fileNamePrivate(const cwCave* cave) const
{
    return sanitizeFileName(cave->name() + QStringLiteral(".cwcave"));
}

QString cwSaveLoad::absolutePathPrivate(const cwCave* cave) const
{
    return dirPrivate(cave).absoluteFilePath(fileNamePrivate(cave));
}

QDir cwSaveLoad::dirPrivate(const cwCave* cave) const
{
    if (cave->parentRegion()) {
        return caveDirHelper(dataRootDir(), cave);
    }
    return QDir();
}

QString cwSaveLoad::fileNamePrivate(const cwTrip* trip) const
{
    return sanitizeFileName(trip->name() + QStringLiteral(".cwtrip"));
}

QString cwSaveLoad::absolutePathPrivate(const cwTrip* trip) const
{
    return dirPrivate(trip).absoluteFilePath(fileNamePrivate(trip));
}

QDir cwSaveLoad::dirPrivate(const cwTrip* trip) const
{
    if (trip->parentCave()) {
        return tripDirHelper(dirPrivate(trip->parentCave()), trip);
    }
    return QDir();
}

QDir cwSaveLoad::dirPrivate(cwSurveyNoteModel* notes) const
{
    if (notes->parentTrip()) {
        return noteDirHelper(dirPrivate(notes->parentTrip()));
    }
    return QDir();
}

QDir cwSaveLoad::dirPrivate(cwSurveyNoteLiDARModel* notes) const
{
    if (notes->parentTrip()) {
        return noteDirHelper(dirPrivate(notes->parentTrip()));
    }
    return QDir();
}

QString cwSaveLoad::fileNamePrivate(const cwNote* note) const
{
    return sanitizeFileName(note->name() + QStringLiteral(".cwnote"));
}

QString cwSaveLoad::absolutePathPrivate(const cwNote* note) const
{
    return dirPrivate(note).absoluteFilePath(fileNamePrivate(note));
}

QString cwSaveLoad::absolutePathPrivate(const cwNote* note, const QString& imageFilename) const
{
    if (note == nullptr) {
        return QString();
    }

    if (!imageFilename.isEmpty()) {
        return dirPrivate(note).absoluteFilePath(imageFilename);
    }

    return QString();
}

QDir cwSaveLoad::dirPrivate(const cwNote* note) const
{
    if (note->parentTrip()) {
        return noteDirHelper(dirPrivate(note->parentTrip()));
    }
    return QDir();
}

QString cwSaveLoad::fileNamePrivate(const cwNoteLiDAR* note) const
{
    return sanitizeFileName(note->name() + QStringLiteral(".cwnote3d"));
}

QString cwSaveLoad::absolutePathPrivate(const cwNoteLiDAR* note) const
{
    return dirPrivate(note).absoluteFilePath(fileNamePrivate(note));
}

QString cwSaveLoad::absolutePathPrivate(const cwNoteLiDAR* note, const QString& lidarFilename) const
{
    if (note == nullptr) {
        return QString();
    }

    if (lidarFilename.isEmpty()) {
        return QString();
    }

    return dirPrivate(note).absoluteFilePath(lidarFilename);
}

QDir cwSaveLoad::dirPrivate(const cwNoteLiDAR* note) const
{
    if (note->parentTrip()) {
        return noteDirHelper(dirPrivate(note->parentTrip()));
    }
    return QDir();
}

void cwSaveLoad::saveProtoMessage(std::unique_ptr<const google::protobuf::Message> message,
                                  const void* objectId)
{
    Q_ASSERT(message);
    d->saveProtoMessage(this,
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

void cwSaveLoad::Data::saveProtoMessage(
    cwSaveLoad* context,
    std::unique_ptr<const google::protobuf::Message> message,
    const void* objectId)
{
    // auto deferred = std::make_shared<AsyncFuture::Deferred<ResultBase>>();

    Job job(
        objectId,
        Job::Kind::File,
        Job::Action::WriteFile,
        std::shared_ptr<const google::protobuf::Message>(std::move(message))
        );

    addFileSystemJob(job, context);
}


bool cwSaveLoad::isTemporaryProject() const
{
    return d->isTemporary;
}

QString cwSaveLoad::dataRoot() const
{
    return d->projectMetadata.dataRoot;
}

void cwSaveLoad::setDataRoot(const QString &dataRoot)
{
    QString normalized = dataRoot.trimmed();
    if (normalized.isEmpty()) {
        auto region = d->m_regionTreeModel->cavingRegion();
        normalized = defaultDataRoot(region ? region->name() : QString());
    }

    if (d->projectMetadata.dataRoot == normalized) {
        return;
    }

    d->projectMetadata.dataRoot = normalized;
    emit dataRootChanged();
    if (!d->projectFileName.isEmpty()) {
        projectRootDir().mkpath(normalized);
    }

    if (d->saveEnabled && !d->projectFileName.isEmpty()) {
        saveProject(projectRootDir(), d->m_regionTreeModel->cavingRegion());
    }
}

cwSaveLoad::GitMode cwSaveLoad::gitMode() const
{
    return d->projectMetadata.gitMode;
}

void cwSaveLoad::setGitMode(GitMode mode)
{
    if (d->projectMetadata.gitMode == mode) {
        return;
    }

    d->projectMetadata.gitMode = mode;

    if (d->saveEnabled && !d->projectFileName.isEmpty()) {
        saveProject(projectRootDir(), d->m_regionTreeModel->cavingRegion());
    }
}

bool cwSaveLoad::syncEnabled() const
{
    return d->projectMetadata.syncEnabled;
}

void cwSaveLoad::setSyncEnabled(bool enabled)
{
    if (d->projectMetadata.syncEnabled == enabled) {
        return;
    }

    d->projectMetadata.syncEnabled = enabled;

    if (d->saveEnabled && !d->projectFileName.isEmpty()) {
        saveProject(projectRootDir(), d->m_regionTreeModel->cavingRegion());
    }
}

QFuture<void> cwSaveLoad::retire()
{
    if (d->retiring) {
        return d->retireFuture;
    }

    d->retiring = true;
    setSaveEnabled(false);
    disconnectTreeModel();
    d->m_regionTreeModel->setCavingRegion(nullptr);

    auto retireFuture = completeSaveJobs();

    d->retireFuture = retireFuture;
    d->futureToken.addJob(cwFuture(QFuture<void>(d->retireFuture),
                                   QStringLiteral("Finishing saves")));

    AsyncFuture::observe(d->retireFuture)
        .context(this, [this]() {
            deleteLater();
        }).future();

    return d->retireFuture;
}
