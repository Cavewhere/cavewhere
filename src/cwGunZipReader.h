/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGUNZIPREADER_H
#define CWGUNZIPREADER_H

//Qt includes
#include <QByteArray>
#include <QStringList>

//Our includes
#include <cwTask.h>

class cwGunZipReader : public cwTask
{
public:
    cwGunZipReader(QObject* parent = NULL);

    //Inputs
    void setFilename(QString filename);

    //Outputs
    QStringList errors() const;
    bool hasErrors() const;

    QByteArray data() const;

protected:
    virtual void runTask();

private:
    QStringList Errors;
    QByteArray Data;
    QString Filename;
};

/**
  The filename to the gunzip archive that'll be extracted
  */
inline void cwGunZipReader::setFilename(QString filename) {
    Filename = filename;
}

/**
  Get's the errors of the extraction
  */
inline QStringList cwGunZipReader::errors() const {
    return Errors;
}

/**
  Returns true if the reader has errors, else false
  */
inline bool cwGunZipReader::hasErrors() const {
    return !Errors.isEmpty();
}

/**
  Gets the data from the extraction
  */
inline QByteArray cwGunZipReader::data() const {
    return Data;
}

#endif // CWGUNZIPREADER_H
