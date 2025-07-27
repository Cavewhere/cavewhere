#include "cwSaveLoad.h"
#include "cwTrip.h"
#include "cwRegionSaveTask.h"
#include "cwConcurrent.h"
#include "cwFutureManagerModel.h"
#include "cwTeam.h"
#include "cwCavingRegion.h"
#include "cwProject.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwImageProvider.h"
#include "cavewhereVersion.h"

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
void cwSaveLoad::saveAllFromV6(const QDir &dir, const cwProject* project)
{

    auto makeDir = [](const QDir& rootDir, const QString& name) {
        auto dirName = sanitizeFileName(name);
        rootDir.mkdir(dirName);

        QDir subDir = rootDir;
        subDir.cd(dirName);

        return subDir;
    };

    const QString projectFilename = project->filename();

    cwImageProvider provider;
    provider.setProjectPath(projectFilename);


    auto saveNotes = [makeDir, this, &provider](const QDir& tripDir, const cwSurveyNoteModel* notes) {
        const QDir noteDir = makeDir(tripDir, QStringLiteral("notes"));

        int imageIndex = 1;
        for(const cwNote* note : notes->notes()) {
            cwNote noteCopy;
            noteCopy.setData(note->data());

            if(noteCopy.name().isEmpty()) {
                noteCopy.setName(QString::number(imageIndex));
            }

            saveNote(noteDir, &noteCopy);

            auto imageData = provider.data(note->image().original());
            qDebug() << "ImageData:" << imageData.format() << imageData.size();

            auto filename = noteDir.absoluteFilePath(QStringLiteral("%1.%2")
                                                        .arg(imageIndex)
                                                        .arg(imageData.format().toLower()));

            qDebug() << "Saving note too:" << filename;

            QSaveFile file(filename);
            file.open(QSaveFile::WriteOnly);
            file.write(imageData.data());
            file.commit();

            ++imageIndex;
        }
    };

    auto saveTrips = [this, projectFilename, makeDir, saveNotes](const QDir& caveDir, const cwCave* cave) {
        const QDir tripsDir = makeDir(caveDir, QStringLiteral("trips"));

        for(const auto trip : cave->trips()) {
            const QDir tripDir = makeDir(tripsDir, trip->name());
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
        const QDir caveDir = makeDir(dir, cave->name());
        saveCave(caveDir, cave);
        saveTrips(caveDir, cave);
    }
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
