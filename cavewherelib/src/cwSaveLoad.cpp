#include "cwSaveLoad.h"
#include "cwTrip.h"
#include "cwRegionSaveTask.h"
#include "cwConcurrent.h"
#include "cwFutureManagerModel.h"
#include "cwTeam.h"

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

};

cwSaveLoad::~cwSaveLoad() = default;

cwSaveLoad::cwSaveLoad(QObject *parent) :
    QObject(parent),
    d(std::make_unique<cwSaveLoad::Data>())
{

}

void cwSaveLoad::saveTrip(const QDir &dir, const cwTrip *trip)
{
    auto protoTrip = toProtoTrip(trip);

    const QString filename = QStringLiteral("trip_data.json");
    d->saveProtoMessage(this, dir.absoluteFilePath(filename), std::move(protoTrip));
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

void cwSaveLoad::Data::saveProtoMessage(
    cwSaveLoad* context,
    const QString &filename,
    std::unique_ptr<const google::protobuf::Message> message)
{
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
