#include "cwSaveLoad.h"
#include "cwTrip.h"
#include "cwRegionSaveTask.h"
#include "cwRegionLoadTask.h"
#include "cwConcurrent.h"
#include "cwFutureManagerModel.h"
#include "cwTeam.h"
#include "cwCavingRegion.h"
#include "cwProject.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwImageProvider.h"
#include "cavewhereVersion.h"
#include "cwPlanScrapViewMatrix.h"
#include "cwRunningProfileScrapViewMatrix.h"
#include "cwRegionTreeModel.h"
#include "cwScrap.h"

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

//QQuickGit
#include "GitRepository.h"

//Monad
#include "Monad/Monad.h"

using namespace Monad;

static QStringList fromProtoStringList(const google::protobuf::RepeatedPtrField<std::string> &protoStringList);




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

struct cwSaveLoad::Data {

    struct RenameJob {
        enum class Kind { File, Directory };

        QString oldPath;
        QString newPath;
        Kind kind = Kind::File;

        bool rename() const {
            qDebug() << "Rename " << oldPath << "->" << newPath;
            return QDir().rename(oldPath, newPath);
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


    QString projectFileName;

    //Where the objects are currently being saved
    //This the absolute directory to the m_rootDir
    QHash<const QObject*, QString> m_fileLookup;

    //For watching when object data has changed
    cwRegionTreeModel* m_regionTreeModel;

    //New file future
    QFuture<void> newFileFuture;

    //Saving jobs
    QList<RenameJob> m_renameJobs;
    std::unordered_map<QString, WaitingJob> m_waitingJobs;
    QHash<QString, QFuture<ResultBase>> m_runningJobs;

    bool isTemporary = true;

    static QList<cwSurveyChunkData> fromProtoSurveyChunks(const google::protobuf::RepeatedPtrField<CavewhereProto::SurveyChunk> & protoList);

    void addRenameJob(const RenameJob& job) {
        Q_ASSERT((job.kind == RenameJob::Kind::Directory) == QFileInfo(job.oldPath).isDir());

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

        auto kindRank = [](RenameJob::Kind kind) -> int {
            // Files first (0), directories second (1).
            return (kind == RenameJob::Kind::File) ? 0 : 1;
        };

        auto lessThan = [&](const RenameJob& a, const RenameJob& b) -> bool {
            const int aRank = kindRank(a.kind);
            const int bRank = kindRank(b.kind);
            if (aRank != bRank) {
                return aRank < bRank;
            }

            const int aDepth = pathDepth(a.oldPath);
            const int bDepth = pathDepth(b.oldPath);
            if (aDepth != bDepth) {
                // Deeper first.
                return aDepth > bDepth;
            }

            // Stable lexical tie-breaker.
            return a.oldPath < b.oldPath;
        };

        const auto beginIt = m_renameJobs.begin();
        const auto endIt   = m_renameJobs.end();

        const auto it = std::lower_bound(beginIt, endIt, job, lessThan);

        // If an element with the same oldPath already exists at this position, replace it.
        if (it != endIt && it->oldPath == job.oldPath) {
            *it = job;
            return;
        }

        // Otherwise insert at the correct sorted position.
        m_renameJobs.insert(static_cast<int>(it - beginIt), job);
    }


    QFuture<Monad::ResultBase> saveProtoMessage(
        cwSaveLoad* context,
        const QString& filename,
        std::unique_ptr<const google::protobuf::Message> message
        );

    QFuture<Monad::ResultBase> saveProtoMessage(
        cwSaveLoad* context,
        const QDir& dir,
        const QString& filename,
        std::unique_ptr<const google::protobuf::Message> message)
    {
        Q_ASSERT(message);
        return saveProtoMessage(context, dir.absoluteFilePath(filename), std::move(message));
    }

    void renameFiles() {
        Q_ASSERT(m_runningJobs.isEmpty());

        for(const auto& job : m_renameJobs) {
            bool couldRename = job.rename();

            qDebug() << "CouldRename:" << couldRename;

            //Remove waiting job, because the should re-queue for save with the new name
            // m_waitingJobs.at(job.oldPath).jobDeferred.cancel();
            m_waitingJobs.erase(job.oldPath);
        }

        m_renameJobs.clear();
    }

    template<typename T>
    void saveObject(cwSaveLoad* context, const T* object) {
        // qDebug() << "Saving object:" << object << object->name() << dir(object);
        auto filename = context->absolutePath(object);
        context->d->m_fileLookup[object] = filename;
        auto saveFuture = context->save(dir(object), object);

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
        auto currentFileName = m_fileLookup.value(object);
        QString folderName = QFileInfo(currentFileName).absolutePath();

        auto fileRename = Data::RenameJob {currentFileName, absolutePath(object), Data::RenameJob::Kind::File};
        auto folderRename = Data::RenameJob {folderName, dir(object).absolutePath(), Data::RenameJob::Kind::Directory};

        addRenameJob(fileRename);
        addRenameJob(folderRename);

        context->save(object);
    }
};

cwSaveLoad::~cwSaveLoad() = default;

cwSaveLoad::cwSaveLoad(QObject *parent) :
    QObject(parent),
    d(std::make_unique<cwSaveLoad::Data>())
{
    d->m_regionTreeModel = new cwRegionTreeModel(this);

    // newProject();

    // connect(d->m_regionTreeModel, &cwRegionTreeModel::rowsInserted,
    //         this, [this](QModelIndex parent, int begin, int end)
    //         {
    //     qDebug() << "Added :" << parent << begin << end;
    //             if(parent == QModelIndex()) {
    //                 //These are all caves
    //                 for(int i = begin; i <= end; i++) {
    //                     auto index = d->m_regionTreeModel->index(i, 0, parent);
    //                     cwCave* cave = index.data(cwRegionTreeModel::ObjectRole).value<cwCave*>();
    //                     d->connectCave(this, cave);
    //                     saveCave(cave);
    //                 }
    //             }
    //         });

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

        d->newFileFuture =
            AsyncFuture::observe(future)
                .context(this, [this]() {
                    //Clear the current data
                    auto region = d->m_regionTreeModel->cavingRegion();

                    if(region) [[likely]] {
                        region->clearCaves();

                        //Rename the region
                        const auto tempName = randomName();
                        region->setName(tempName);

                        //Create the temp directory
                        auto tempDir = createTemporaryDirectory(tempName);

                        //Add repository
                        QQuickGit::GitRepository repo;
                        repo.setDirectory(tempDir);
                        repo.initRepository();

                        //Save the project file
                        saveCavingRegion(tempDir, region);

                        //Connect all for watching for saves
                        connectTreeModel();

                        setTemporary(true);
                        setFileNameHelper(tempDir.absoluteFilePath(regionFileName(region)));
                    }
                }).future();
    }
}

QString cwSaveLoad::fileName() const
{
    return d->projectFileName;
}

void cwSaveLoad::setFileName(const QString &filename)
{
    d->projectFileName = filename;
}

void cwSaveLoad::setCavingRegion(cwCavingRegion *region)
{
    d->m_regionTreeModel->setCavingRegion(region);
}

const cwCavingRegion *cwSaveLoad::cavingRegion() const
{
    return d->m_regionTreeModel->cavingRegion();
}

QFuture<ResultBase> cwSaveLoad::saveCavingRegion(const QDir &dir, const cwCavingRegion *region)
{
    return saveProtoMessage(dir,
                            regionFileName(region),
                            toProtoCavingRegion(region)
                            );
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
        toProtoCave(cave));
}

std::unique_ptr<CavewhereProto::Cave> cwSaveLoad::toProtoCave(const cwCave *cave)
{
    auto protoCave = std::make_unique<CavewhereProto::Cave>();
    *(protoCave->mutable_name()) = cave->name().toStdString();
    return protoCave;
}

QFuture<ResultBase> cwSaveLoad::save(const QDir &dir, const cwTrip *trip)
{
    return saveProtoMessage(
        dir,
        fileName(trip),
        toProtoTrip(trip));
}

std::unique_ptr<CavewhereProto::Trip> cwSaveLoad::toProtoTrip(const cwTrip *trip)
{
    //Copy trip data into proto, on the main thread
    auto protoTrip = std::make_unique<CavewhereProto::Trip>();

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

QFuture<ResultBase> cwSaveLoad::save(const QDir &dir, const cwNote *note)
{
    return saveProtoMessage(
        dir,
        fileName(note),
        toProtoNote(note));
}

std::unique_ptr<CavewhereProto::Note> cwSaveLoad::toProtoNote(const cwNote *note)
{
    //Copy trip data into proto, on the main thread
    auto protoNote = std::make_unique<CavewhereProto::Note>();

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


        qDebug() << "Saving image:" << filename;

        QSaveFile file(filename);
        file.open(QSaveFile::WriteOnly);
        file.write(imageData.data());
        file.commit();

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
                    QDirIterator tripIt(tripsDir.absolutePath(), QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
                    while (tripIt.hasNext()) {
                        tripIt.next();
                        QDir tripDir(tripIt.filePath());

                        QFileInfoList tripFiles = tripDir.entryInfoList(QStringList() << "*.cwtrip", QDir::Files);
                        for (const QFileInfo &tripFileInfo : tripFiles) {
                            auto tripResult = loadTrip(tripFileInfo.absoluteFilePath());

                            if (tripResult.hasError()) {
                                // FIXME: log or collect the error
                                continue;
                            }

                            cwTripData trip = tripResult.value();

                            // Load notes inside the notes subfolder
                            QDir notesDir = tripDir.filePath("notes");
                            if (notesDir.exists()) {
                                QFileInfoList noteFiles = notesDir.entryInfoList(QStringList() << "*.cwnote", QDir::Files);
                                for (const QFileInfo &noteFileInfo : noteFiles) {
                                    auto noteResult = loadNote(noteFileInfo.absoluteFilePath(), regionDir);
                                    if (noteResult.hasError()) {
                                        // FIXME: log or collect the error
                                        continue;
                                    }

                                    trip.noteModel.notes.append(noteResult.value());
                                }
                            }

                            cave.trips.append(trip);
                        }
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

        // Load the image
        noteData.image.setPath(QString::fromStdString(protoNote.image().path()));
        noteData.image.setOriginalSize(cwRegionLoadTask::loadSize(protoNote.image().size()));
        noteData.image.setOriginalDotsPerMeter(protoNote.image().dotpermeter());

        // Load scraps
        for (const auto& protoScrap : protoNote.scraps()) {
            auto scrap = fromProtoScrap(protoScrap);
            noteData.scraps.append(scrap);
        }

        noteData.name = QString::fromStdString(protoNote.name());

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

cwLength::Data cwSaveLoad::fromProtoLength(const CavewhereProto::Length &protoLength)
{
    return {
        protoLength.unit(),
        protoLength.value()
    };
}

void cwSaveLoad::waitForFinished()
{
    auto waitOnRunningJobs = [this]() {
        cwFutureManagerModel model;
        for(auto it = d->m_runningJobs.begin();
             it != d->m_runningJobs.end();
             ++it)
        {
            model.addJob(cwFuture(QFuture<void>(it.value()), QStringLiteral()));
        }
        model.addJob(cwFuture(d->newFileFuture, QStringLiteral()));
        model.waitForFinished();
    };

    auto future = AsyncFuture::observe(d->newFileFuture)
                      .context(this, waitOnRunningJobs)
                      .future();

    waitOnRunningJobs();
}

void cwSaveLoad::disconnectTreeModel()
{
    //Disconnect from the tree model
    disconnect(d->m_regionTreeModel, nullptr, this, nullptr);

    //Disconnect all the objects watch in the tree
    disconnectObjects();
}

void cwSaveLoad::connectTreeModel()
{

    //Connect when region has changed
    connect(d->m_regionTreeModel, &cwRegionTreeModel::rowsInserted,
            this, [this](const QModelIndex &parent, int first, int last) {
                for(int i = first; i <= last; i++) {
                    auto index = d->m_regionTreeModel->index(i, 0, parent);
                    switch(index.data(cwRegionTreeModel::TypeRole).toInt()) {
                    case cwRegionTreeModel::CaveType: {
                        auto cave = d->m_regionTreeModel->cave(index);
                        save(cave);
                        connectCave(cave);
                        break;
                    }
                    case cwRegionTreeModel::TripType: {
                        auto trip = d->m_regionTreeModel->trip(index);
                        save(trip);
                        connectTrip(trip);
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
                    default:
                        break;
                    }
        }
    });

    connect(d->m_regionTreeModel, &cwRegionTreeModel::rowsAboutToBeRemoved,
            this, [this](const QModelIndex &parent, int first, int last) {
                for(int i = first; i <= last; i++) {
                    auto index = d->m_regionTreeModel->index(i, 0, parent);
                    auto object = index.data(cwRegionTreeModel::ObjectRole).value<QObject*>();
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

}

void cwSaveLoad::connectCave(cwCave *cave)
{
    if (cave == nullptr) {
        return;
    }

    auto saveCaveName = [cave, this]() {
        d->renameDirectoryAndFile(this, cave);
    };

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
        connect(teamModel, &QAbstractItemModel::dataChanged, this, saveTrip);
        connect(teamModel, &QAbstractItemModel::rowsInserted, this, saveTrip);
        connect(teamModel, &QAbstractItemModel::rowsRemoved, this, saveTrip);
        connect(teamModel, &QAbstractItemModel::modelReset, this, saveTrip);
        connect(teamModel, &QAbstractItemModel::layoutChanged, this, saveTrip);
    }

    // Notes model changes → save
    // if (QAbstractItemModel* const notesModel = trip->notes()) {
    //     connect(notesModel, &QAbstractItemModel::dataChanged, this, saveTrip);
    //     connect(notesModel, &QAbstractItemModel::rowsInserted, this, saveTrip);
    //     connect(notesModel, &QAbstractItemModel::rowsRemoved, this, saveTrip);
    //     connect(notesModel, &QAbstractItemModel::modelReset, this, saveTrip);
    //     connect(notesModel, &QAbstractItemModel::layoutChanged, this, saveTrip);
    // }

    // Trip calibration changes → save
    if (cwTripCalibration* const cal = trip->calibrations()) {
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
        auto currentFileName = d->m_fileLookup.value(note);
        auto fileRename = Data::RenameJob {currentFileName, absolutePath(note), Data::RenameJob::Kind::File};
        d->addRenameJob(fileRename);

        save(note);
    };

    // Note-level changes
    connect(note, &cwNote::nameChanged, this, renameAndSaveNote);

    // Lambda to save this specific note
    const auto saveNote = [this, note]() {
        save(note);
    };
    connect(note, &cwNote::imageChanged, this, saveNote);
    connect(note, &cwNote::rotateChanged, this, saveNote);
    connect(note, &cwNote::imageResolutionChanged, this, saveNote);

    // connect(note, &cwNote::beginInsertingScraps, this, saveNote);
    connect(note, &cwNote::insertedScraps, this, saveNote);
    // connect(note, &cwNote::beginRemovingScraps, this, saveNote);
    connect(note, &cwNote::removedScraps, this, saveNote);
    // connect(note, &cwNote::scrapAdded, this, saveNote);
    connect(note, &cwNote::scrapsReset, this, saveNote);
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
}


void cwSaveLoad::setFileNameHelper(const QString &fileName)
{
    if(d->projectFileName != fileName) {
        d->projectFileName = fileName;
        emit fileNameChanged();
    }
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

QString cwSaveLoad::fileName(const cwNote *note)
{
    return sanitizeFileName(note->name() + QStringLiteral(".cwnote"));
}

QString cwSaveLoad::absolutePath(const cwNote *note)
{
    return dir(note).absoluteFilePath(fileName(note));
}

QDir cwSaveLoad::dir(const cwNote *note)
{
    if(note->parentTrip()) {
        return noteDirHelper(dir(note->parentTrip()));
    } else {
        return QDir();
    }
}

QFuture<ResultBase> cwSaveLoad::saveProtoMessage(const QDir &dir,
                                  const QString &filename,
                                  std::unique_ptr<const google::protobuf::Message> message)
{
    Q_ASSERT(message);
    return d->saveProtoMessage(this,
                               dir.absoluteFilePath(sanitizeFileName(filename)),
                               std::move(message));

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
    std::unique_ptr<const google::protobuf::Message> message)
{
    qDebug () << "Try Saving to " << filename;

    if(!m_renameJobs.isEmpty() && m_runningJobs.isEmpty()) {
        //Rename files and directories
        renameFiles();
    }

    //Make sure we're not saving to the same file at the same time
    if (m_runningJobs.contains(filename) || !m_renameJobs.isEmpty()) {

        qDebug() << "\twaiting runningJob:" << m_runningJobs.contains(filename) << "rename:" << !m_renameJobs.isEmpty();

        auto deferred = AsyncFuture::Deferred<ResultBase>();

        m_waitingJobs[filename] = WaitingJob {
            deferred,
            std::move(message),
        };

        //Add defered here
        return deferred.future();
    } else {

        auto future = cwConcurrent::run([filename, message = std::move(message)]() {
            auto ensurePathForFile = [](const QString& filePath) {
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
            };

            return mbind(ensurePathForFile(filename), [&](ResultBase /*result*/) {
                QSaveFile file(filename);
                qDebug() << "Saving:" << filename;
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

                return Monad::ResultBase();
            });
        });

        AsyncFuture::observe(future).context(
            context, [filename, this, context]() {
                qDebug() << "job finished:" << filename;
                m_runningJobs.remove(filename);

                auto runWaitingJobs = [this, context]() {
                    Q_ASSERT(m_renameJobs.isEmpty());
                    Q_ASSERT(m_runningJobs.isEmpty());
                    // Run waiting save jobs
                    for (auto it = m_waitingJobs.begin(); it != m_waitingJobs.end(); ++it) {
                        WaitingJob& job = it->second;

                        //Recursive call
                        auto future = saveProtoMessage(context, it->first, std::move(job.message));
                        AsyncFuture::observe(future).context(context, [deferred = std::move(job.jobDeferred), future]() mutable {
                            deferred.complete(future.result());
                        });
                    }
                    m_waitingJobs.clear();
                };

                if(m_renameJobs.isEmpty() && m_runningJobs.isEmpty()) {
                    runWaitingJobs();
                } else if(m_runningJobs.isEmpty()) {
                    renameFiles();
                    runWaitingJobs();
                }
            });

        m_runningJobs.insert(filename, future);

        return future;
    }
}


bool cwSaveLoad::isTemporaryProject() const
{
    return d->isTemporary;
}
