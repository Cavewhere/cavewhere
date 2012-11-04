//Our includes
#include "cwGunZipReader.h"

//Zlib includes
#include <zlib.h>

//Qt includes
#include <QFileInfo>

cwGunZipReader::cwGunZipReader(QObject* parent) :
    cwTask(parent)
{
}

/**
  This extracts the data from the filename places it into Data

  call data() after this task isn't running to extract the data.
  call errors() to see if this task has errored
  */
void cwGunZipReader::runTask() {
    Data.clear();
    Errors.clear();

    if(!QFileInfo(Filename).exists()) {
        Errors.append(QString("Can't parse plot sauce xml file becauce the file doesn't exist: %1").arg(Filename));
        stop();
        done();
        return;
    }

    QByteArray buffer;
    const int bufferSize = 32 * 1024;
    buffer.resize(bufferSize); //32k buffer

    gzFile file = gzopen((const char*)Filename.toLocal8Bit(), "r");
    int numberOfBytesCopied = 0;
    while((numberOfBytesCopied = gzread(file, buffer.data(), buffer.size())) && isRunning()) {
        buffer.resize(numberOfBytesCopied);
        Data.append(buffer);
    }

    if(!isRunning()) {
        Data.clear();
    }

    //If there was a error reading
    if(!gzeof(file)) {
        int errorCode;
        const char* errorString = gzerror(file, &errorCode);
        Errors.append(QString("There was an error reading gunzip data: %1 %2").arg(errorCode).arg(errorString));
        stop();
    }

    done();
}
