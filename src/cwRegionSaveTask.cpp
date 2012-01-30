//Our includes
#include "cwRegionSaveTask.h"
#include "cwRegionIOTask.h"

//Serielization includes
#include "cwSerialization.h"
#include "cwQtSerialization.h"

//Qt includes
#include <QSqlQuery>
#include <QSqlError>

//Std includes
#include <sstream>

cwRegionSaveTask::cwRegionSaveTask(QObject *parent) :
    cwRegionIOTask(parent)
{
}

void cwRegionSaveTask::runTask() {

    //Open a datebase connection
    bool connected = connectToDatabase("saveRegionTask");
    if(connected) {

        //Serielize the Region to XML data
        std::stringstream stream;
        boost::archive::xml_oarchive xmlSaveArchive(stream);
        xmlSaveArchive << BOOST_SERIALIZATION_NVP(Region);

        //Write the data to the database
       writeXMLToDatabase(QString::fromStdString(stream.str()));

        Database.close();
    }

    //Clear the region of data
    *Region = cwCavingRegion();

    //Finished
    done();

}

/**
  Writes the xml data to the database
  */
void cwRegionSaveTask::writeXMLToDatabase(QString xml) {

    QSqlQuery insertCavingRegion(Database);
    QString queryStr =
            QString("INSERT OR REPLACE INTO CavingRegion ") +
            QString("(id, qCompress_XML) ") +
            QString("VALUES (1, ?)");

    bool successful = insertCavingRegion.prepare(queryStr);
    if(!successful) {
        qDebug() << "Couldn't create query to insert region xml data:" << insertCavingRegion.lastError();
        stop();
    }

    insertCavingRegion.bindValue(0, xml);
    insertCavingRegion.exec();
}
