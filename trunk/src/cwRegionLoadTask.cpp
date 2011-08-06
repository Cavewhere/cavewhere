//Our includes
#include "cwRegionLoadTask.h"

//Serielization includes
#include "cwSerialization.h"
#include "cwQtSerialization.h"

//Boost includes
#include <boost/archive/xml_archive_exception.hpp>

//Qt includes
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>

//Std includes
#include <sstream>

cwRegionLoadTask::cwRegionLoadTask(QObject *parent) :
    cwRegionIOTask(parent)
{

}

/**
  Loads the region data
  */
void cwRegionLoadTask::runTask() {

    //Clear region
    bool connected = connectToDatabase("loadRegionTask");
    if(connected) {
        //Get the xmlData from the database
        QString xmlData = readXMLFromDatabase();
        Database.close();

        //Add the string data to the stream
        std::stringstream stream(xmlData.toStdString());

        //Parse xml the string data
        try {
            boost::archive::xml_iarchive xmlLoadArchive(stream);

            cwCavingRegion region;
            xmlLoadArchive >> BOOST_SERIALIZATION_NVP(region);
            *Region = region;
        } catch(boost::archive::xml_archive_exception exception) {
            qDebug() << "Couldn't load data!";
            stop();
        }
    }

    if(isRunning()) {
        emit finishedLoading(Region);
    }

    done();
}

/**
  Reads the region data from the database

  This return a QString filed with XML data
  */
QString cwRegionLoadTask::readXMLFromDatabase() {
    QSqlQuery selectCavingRegion(Database);
    QString queryStr =
            QString("SELECT qCompress_XML FROM CavingRegion where id = 1");

    bool couldPrepare = selectCavingRegion.prepare(queryStr);
    if(!couldPrepare) {
        qDebug() << "Couldn't prepare select Caving Region:" << selectCavingRegion.lastError().databaseText() << queryStr;
    }

    //Extract the data
    selectCavingRegion.exec();
    QSqlRecord record = selectCavingRegion.record();

    if(record.isEmpty()) {
        qDebug() << "Hmmmm, no caving regions to load";
        stop();
        return QString();
    }

    //Get the first row
    selectCavingRegion.next();

//    qDebug() << "Record:" << record << "Value:" << selectCavingRegion.value(0);

    QByteArray xmlData = selectCavingRegion.value(0).toByteArray();
    return QString(xmlData);
}
