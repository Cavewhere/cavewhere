/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLinkGenerator.h"
#include "cwPageSelectionModel.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwDebug.h"
#include "cwSurveyNoteModel.h"
#include "cwPage.h"


cwLinkGenerator::cwLinkGenerator(QObject *parent) : QObject(parent)
{

}

cwLinkGenerator::~cwLinkGenerator()
{

}


/**
 * @brief cwLinkGenerator::dataPageLink
 * @return The address of the top-level Data page (parent of the per-cave pages).
 */
QString cwLinkGenerator::dataPageLink()
{
    return QStringLiteral("Source")
           + cwPageSelectionModel::seperator()
           + QStringLiteral("Data");
}

/**
 * @brief cwLinkGenerator::link
 * @param cave
 * @return
 */
QString cwLinkGenerator::caveLink(cwCave *cave)
{
    if(cave == nullptr) { return QString(); }
    return dataPageLink()
           + cwPageSelectionModel::seperator()
           + QStringLiteral("Cave=")
           + cave->name();
}

/**
 * @brief cwLinkGenerator::link
 * @param cave
 * @return
 */
QString cwLinkGenerator::tripLink(cwTrip *trip)
{
    if(trip == nullptr) { return QString(); }
    return caveLink(trip->parentCave())
           + cwPageSelectionModel::seperator()
           + QStringLiteral("Trip=")
           + trip->name();

}

/**
 * @brief cwLinkGenerator::link
 * @param cave
 * @return
 */
QString cwLinkGenerator::scrapLink(cwScrap *scrap)
{
    if(scrap == nullptr) { return QString(); }
    return noteLink(scrap->parentNote());
}

/**
 * @brief cwLinkGenerator::link
 * @param cave
 * @return
 */
QString cwLinkGenerator::noteLink(cwNote *note)
{
    if(note == nullptr) { return QString(); }
    return tripLink(note->parentTrip())
           + cwPageSelectionModel::seperator()
           + QStringLiteral("Note=")
           + note->name();
}

/**
* @brief cwLinkGenerator::pageSelectionModel
* @return Returns the current page selection model
*/
cwPageSelectionModel* cwLinkGenerator::pageSelectionModel() const {
    return PageSelectionModel;
}

/**
* @brief cwLinkGenerator::setPageSelectionModel
* @param pageSelectionModel - Sets the current selection model
*/
void cwLinkGenerator::setPageSelectionModel(cwPageSelectionModel* pageSelectionModel) {
    if(PageSelectionModel != pageSelectionModel) {
        PageSelectionModel = pageSelectionModel;
        emit pageSelectionModelChanged();
    }
}

/**
 * @brief cwLinkGenerator::gotoScrap
 * @param scrap
 */
void cwLinkGenerator::gotoScrap(cwScrap *scrap)
{
    gotoNote(scrap->parentNote());
}

/**
 * @brief cwLinkGenerator::gotoNote
 * @param note
 */
void cwLinkGenerator::gotoNote(cwNote *note)
{
    if(note == nullptr) { return; }

    if(pageSelectionModel() == nullptr) {
        qDebug() << "Can't goto note" << note << "because pageSelectionModel() is null" << LOCATION;
        return;
    }

    QString link = noteLink(note);
    pageSelectionModel()->setCurrentPageAddress(link);
}

