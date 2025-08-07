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

//Monad
#include "Monad/Monad.h"

using namespace Monad;

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
    QDir m_rootDir; //Root project directory

    QHash<QString, QFuture<void>> m_runningJobs;
    std::unordered_map<QString, std::unique_ptr<const google::protobuf::Message>> m_waitingJobs;

    void saveProtoMessage(
        cwSaveLoad* context,
        const QString& filename,
        std::unique_ptr<const google::protobuf::Message> message
        );

    void saveProtoMessage(
        cwSaveLoad* context,
        const QDir& dir,
        const QString& filename,
        std::unique_ptr<const google::protobuf::Message> message)
    {
        if(message) {
            saveProtoMessage(context, dir.absoluteFilePath(filename), std::move(message));
        }
    }

};

cwSaveLoad::~cwSaveLoad() = default;

cwSaveLoad::cwSaveLoad(QObject *parent) :
    QObject(parent),
    d(std::make_unique<cwSaveLoad::Data>())
{

}

void cwSaveLoad::saveCavingRegion(const QDir &dir, const cwCavingRegion *region)
{
    saveProtoMessage(dir,
                     sanitizeFileName(QStringLiteral("%1.cw").arg(region->name())),
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

void cwSaveLoad::saveCave(const QDir &dir, const cwCave *cave)
{
    saveProtoMessage(
        dir,
        QStringLiteral("%1.cwcave").arg(cave->name()),
        toProtoCave(cave));
}

std::unique_ptr<CavewhereProto::Cave> cwSaveLoad::toProtoCave(const cwCave *cave)
{
    auto protoCave = std::make_unique<CavewhereProto::Cave>();
    *(protoCave->mutable_name()) = cave->name().toStdString();
    return protoCave;
}

void cwSaveLoad::saveTrip(const QDir &dir, const cwTrip *trip)
{
    saveProtoMessage(
        dir,
        QStringLiteral("%1.cwtrip").arg(trip->name()),
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

void cwSaveLoad::saveNote(const QDir &dir, const cwNote *note)
{
    saveProtoMessage(
        dir,
        QStringLiteral("%1.cwnote").arg(note->name()),
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

    return protoNote;
}


/**
 * This saves all the data in region into directory
 *
 * This is useful for converting older CaveWhere files to the new file format
 */
QString cwSaveLoad::saveAllFromV6(const QDir &dir, const cwProject* project)
{

    // auto makeDir = [](const QDir& rootDir, const QString& name) {
    //     auto dirName = sanitizeFileName(name);
    //     rootDir.mkdir(dirName);

    //     QDir subDir = rootDir;
    //     subDir.cd(dirName);

    //     return subDir;
    // };

    auto makeDir = [](const QDir& dir) {
        dir.mkpath(QStringLiteral("."));
        return dir;
    };

    const QString projectFilename = project->filename();

    qDebug() << "Project path for image provider:" << projectFilename;

    cwImageProvider provider;
    provider.setProjectPath(projectFilename);


    auto saveNotes = [makeDir, this, &provider, dir](const QDir& tripDir, const cwSurveyNoteModel* notes) {
        const QDir noteDir = makeDir(noteDirHelper(tripDir));

        int imageIndex = 1;
        for(const cwNote* note : notes->notes()) {
            cwNote noteCopy;
            noteCopy.setData(note->data());

            if(noteCopy.name().isEmpty()) {
                noteCopy.setName(QString::number(imageIndex));
            }
            auto imageData = provider.data(note->image().original());
            qDebug() << "ImageData:" << note->image().original() << imageData.format() << imageData.size();

            auto filename = noteDir.absoluteFilePath(QStringLiteral("%1.%2")
                                                         .arg(imageIndex)
                                                         .arg(imageData.format().toLower()));

            qDebug() << "Saving note too:" << filename;

            QSaveFile file(filename);
            file.open(QSaveFile::WriteOnly);
            file.write(imageData.data());
            file.commit();


            cwImage noteImage = noteCopy.image();
            QString relativeFilename = dir.relativeFilePath(filename);
            noteImage.setPath(relativeFilename);
            noteCopy.setImage(noteImage);

            saveNote(noteDir, &noteCopy);

            ++imageIndex;
        }
    };

    auto saveTrips = [this, projectFilename, makeDir, saveNotes](const QDir& caveDir, const cwCave* cave) {
        // const QDir tripsDir = makeDir(caveDir, QStringLiteral("trips"));

        for(const auto trip : cave->trips()) {
            const QDir tripDir = makeDir(tripDirHelper(caveDir, trip));
            saveTrip(tripDir, trip);
            saveNotes(tripDir, trip->notes());
        }
    };

    //Save the region's data
    cwCavingRegion region;
    region.setName(QFileInfo(project->filename()).baseName());
    saveCavingRegion(dir, &region);

    //Go through all the caves
    for(const auto cave : project->cavingRegion()->caves()) {
        const QDir caveDir = makeDir(caveDirHelper(dir, cave));
        saveCave(caveDir, cave);
        saveTrips(caveDir, cave);
    }

    return regionFileName(dir, &region);


}

Monad::Result<cwCavingRegionData> cwSaveLoad::loadAll(const QString &filename)
{
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
    qDebug() << "Loading cave:" << filename;
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
    qDebug() << "Loading trip:" << filename;
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

                            tripData.chunks = fromProtoSurveyChunks(tripProto.chunks());


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

QList<cwSurveyChunkData> cwSaveLoad::fromProtoSurveyChunks(const google::protobuf::RepeatedPtrField<CavewhereProto::SurveyChunk> &protoList)
{
    QList<cwSurveyChunkData> chunks;

    if(!protoList.empty()) {
        chunks.reserve(protoList.size());

        for (const auto& protoChunk : protoList) {
            chunks.append(fromProtoSurveyChunk(protoChunk));
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
        case CavewhereProto::Scrap::ScrapType::Scrap_ScrapType_RunningProfile:
            scrapData.viewMatrix = std::make_unique<cwRunningProfileScrapViewMatrix::Data>();
        case CavewhereProto::Scrap::ScrapType::Scrap_ScrapType_ProjectedProfile:
            // Load view matrix
            if (protoScrap.has_profileviewmatrix()) {
                scrapData.viewMatrix = fromProtoProjectedScraptViewMatrix(protoScrap.profileviewmatrix());
            } else {
                scrapData.viewMatrix = std::make_unique<cwProjectedProfileScrapViewMatrix::Data>();
            }
            scrapData.viewMatrix = std::make_unique<cwProjectedProfileScrapViewMatrix::Data>();
        default:
            scrapData.viewMatrix = std::make_unique<cwPlanScrapViewMatrix::Data>();
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
    cwFutureManagerModel model;
    for(auto it = d->m_runningJobs.begin();
         it != d->m_runningJobs.end();
         ++it)
    {
        model.addJob(cwFuture(it.value(), QStringLiteral()));
    }
    model.waitForFinished();
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


QStringList cwSaveLoad::fromProtoStringList(const google::protobuf::RepeatedPtrField<std::string>& protoStringList)
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

QDir cwSaveLoad::caveDir(const cwCave *cave)
{
    if(cave->parentRegion() && cave->parentRegion()->parentProject()) {
        QDir projDir = projectDir(cave->parentRegion()->parentProject());
        return caveDirHelper(projDir, cave);
    } else {
        return QDir();
    }

}

QDir cwSaveLoad::tripDir(const cwTrip *trip)
{
    if(trip->parentCave()) {
        return tripDirHelper(caveDir(trip->parentCave()), trip);
    } else {
        return QDir();
    }
}

QDir cwSaveLoad::noteDir(const cwNote *note)
{
    if(note->parentTrip()) {
        return noteDirHelper(tripDir(note->parentTrip()));
    } else {
        return QDir();
    }
}

void cwSaveLoad::saveProtoMessage(const QDir &dir,
                                  const QString &filename,
                                  std::unique_ptr<const google::protobuf::Message> message)
{
    if(message) {
        d->saveProtoMessage(this,
                            dir.absoluteFilePath(sanitizeFileName(filename)),
                            std::move(message));
    }
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
    const QString &filename,
    std::unique_ptr<const google::protobuf::Message> message)
{
    qDebug() << "Filename:" << filename;

    //Make sure we're not saving to the same file at the same time
    if (m_runningJobs.contains(filename)) {
        m_waitingJobs[filename] = std::move(message);
    } else {
        auto future = cwConcurrent::run([filename, message = std::move(message)]() {
            QSaveFile file(filename);
            if (!file.open(QFile::WriteOnly)) {
                qWarning("Failed to open file for writing: %s", qUtf8Printable(filename));
                return;
            }

            std::string json_output;
            google::protobuf::util::JsonPrintOptions options;
            options.add_whitespace = true; // Makes it pretty
            // options.always_print_primitive_fields = true; // Optional: print even if fields are default

            auto status = google::protobuf::util::MessageToJsonString(*message, &json_output, options);
            if (!status.ok()) {
                qWarning("Failed to convert proto message to JSON: %s", status.ToString().c_str());
                return;
            }

            file.write(json_output.c_str(), json_output.size());
            file.commit();
        });

        AsyncFuture::observe(future).context(context, [filename, this, context]() {
            m_runningJobs.remove(filename);

            //Run waiting save jobs
            for (auto it = m_waitingJobs.begin();
                 it != m_waitingJobs.end();
                 ++it) {
                saveProtoMessage(context, it->first, std::move(it->second));
            }
            m_waitingJobs.clear();
        });

        m_runningJobs.insert(filename, future);
    }

}
