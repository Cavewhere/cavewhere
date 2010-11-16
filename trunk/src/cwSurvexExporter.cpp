//Our includes
#include "cwSurvexExporter.h"
#include "cwSurveyChunkGroup.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"

//Qt includes
#include <QFile>
#include <QTextStream>

cwSurvexExporter::cwSurvexExporter(QObject *parent) :
    QObject(parent),
    ChunkGroup(NULL)
{

}

/**
  \brief Set
  */
void cwSurvexExporter::setChunks(cwSurveyChunkGroup* chunks) {
    if(ChunkGroup != chunks) {
        ChunkGroup = chunks;
    }
}

void cwSurvexExporter::exportSurvex(QString filename) {
    if(ChunkGroup == NULL) { return; }

    //Open the file for writting
    QFile file(filename);
    bool canWrite = file.open(QIODevice::WriteOnly);
    if(!canWrite) { return; }

    //Open the stream
    QTextStream stream(&file);

    //Write the header
    stream << "*begin" << endl;
    stream << "*data normal from to tape compass backcompass clino backclino" << endl;

    QList<cwSurveyChunk*> chunks = ChunkGroup->chunks();
    foreach(cwSurveyChunk* chunk, chunks) {
        writeChunk(stream, chunk);
        stream << endl;
    }

    stream << "*end" << endl;
}

void cwSurvexExporter::writeChunk(QTextStream& stream, cwSurveyChunk* chunk) {
    for(int i = 0; i < chunk->StationCount() - 1; i++) {
        cwStation* fromStation = chunk->Station(i);
        cwStation* toStation = chunk->Station(i + 1);
        cwShot* shot = chunk->Shot(i);

        if(!fromStation->IsValid() || !toStation->IsValid()) { continue; }

        QString distance = shot->GetDistance().toString();
        QString compass = shot->GetCompass().toString();
        QString backCompass = shot->GetBackCompass().toString();
        QString clino = shot->GetClino().toString();
        QString backClino = shot->GetBackClino().toString();

        //Make sure the model is good
        if(distance.isEmpty()) { continue; }
        if(compass.isEmpty() && backCompass.isEmpty()) {
           if(clino.compare("up", Qt::CaseInsensitive) != 0 &&
                   clino.compare("down", Qt::CaseInsensitive) != 0 &&
                   backClino.compare("up", Qt::CaseInsensitive) != 0 &&
                   backClino.compare("down", Qt::CaseInsensitive) != 0) {
               continue;
           }
        }
        if(clino.isEmpty() && backClino.isEmpty()) {
            continue;
        }

        if(compass.isEmpty()) { compass = "-"; }
        if(backCompass.isEmpty()) { backCompass = "-"; }
        if(clino.isEmpty()) { clino = "-"; }
        if(backClino.isEmpty()) { backClino = "-"; }

        stream << fromStation->GetName() << " " << toStation->GetName() << " " << distance << " " << compass << " " << backCompass << " " << clino << " " << backClino << endl;
    }
}


