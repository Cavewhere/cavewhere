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
 * @brief cwLinkGenerator::link
 * @param cave
 * @return
 */
QString cwLinkGenerator::caveLink(cwCave *cave)
{
    if(cave == nullptr) { return QString(); }
    return QString("Data%1Cave=%2")
            .arg(cwPageSelectionModel::seperator())
            .arg(cave->name());
}

/**
 * @brief cwLinkGenerator::link
 * @param cave
 * @return
 */
QString cwLinkGenerator::tripLink(cwTrip *trip)
{
    if(trip == nullptr) { return QString(); }
    QString sep = cwPageSelectionModel::seperator();
    return QString("%1%2Trip=%3")
            .arg(caveLink(trip->parentCave()))
            .arg(sep)
            .arg(trip->name());

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
    return tripLink(note->parentTrip());
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
    if(pageSelectionModel() == nullptr) {
        qDebug() << "Can't goto note" << note << "because pageSelectionModel() is null" << LOCATION;
    }

    QString link = noteLink(note);
    pageSelectionModel()->setCurrentPageAddress(link);
    cwPage* page = pageSelectionModel()->currentPage();

    Q_ASSERT(page != nullptr);

    int index = note->parentTrip()->notes()->notes().indexOf(note);

    QVariantMap selection;
    selection.insert("currentNoteIndex", index);

    page->setSelectionProperties(selection);
}


