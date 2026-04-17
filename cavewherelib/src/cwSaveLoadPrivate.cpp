#include "cwSaveLoadPrivate.h"

//Our includes
#include "cwProtoUtils.h"
#include "cwSurveyChunk.h"

//Google protobuffer
#include "cavewhere.pb.h"

QList<cwSurveyChunkData> cwSaveLoadPrivate::fromProtoSurveyChunks(const google::protobuf::RepeatedPtrField<CavewhereProto::SurveyChunk> &protoList)
{
    QList<cwSurveyChunkData> chunks;

    if(!protoList.empty()) {
        chunks.reserve(protoList.size());

        for (const auto& protoChunk : protoList) {
            chunks.append(cwProtoUtils::fromProtoSurveyChunk(protoChunk));
        }
    }

    return chunks;
}

void cwSaveLoadPrivate::saveProtoMessage(
        cwSaveLoad* context,
        std::unique_ptr<const google::protobuf::Message> message,
        const void* objectId)
{
    Job job(
                objectId,
                Job::Kind::File,
                Job::Action::WriteFile,
                std::shared_ptr<const google::protobuf::Message>(std::move(message))
                );

    addFileSystemJob(job, context);
}
