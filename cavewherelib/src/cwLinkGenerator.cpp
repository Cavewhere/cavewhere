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
#include "cwSurveyNode.h"
#include "cwTrip.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwDebug.h"
#include "cwSurveyNoteModel.h"
#include "cwPage.h"


namespace {
//! Must match the sub-page NodePage.qml registers in registerSubPages().
constexpr QLatin1String kFixStationsPageName("Fix Stations");
//! Must match the page names DataMainPage.qml and NodePage.qml register.
constexpr QLatin1String kNodePagePrefix("Node=");
constexpr QLatin1String kTripPagePrefix("Trip=");
}

cwLinkGenerator::cwLinkGenerator(QObject *parent) : QObject(parent)
{

}

cwLinkGenerator::~cwLinkGenerator()
{

}


/**
 * @brief cwLinkGenerator::dataPageLink
 * @return The address of the top-level Data page (parent of the top-level node pages).
 */
QString cwLinkGenerator::dataPageLink()
{
    return QStringLiteral("Source")
           + cwPageSelectionModel::seperator()
           + QStringLiteral("Data");
}

/**
 * The address of \a node's page: the Data page, then one Node= part per node
 * from the top level down to \a node, so a Section in a Cave in a Folder is
 * Source/Data/Node=Folder/Node=Cave/Node=Section. The region's root node is the
 * Data page itself.
 */
QString cwLinkGenerator::nodeLink(cwSurveyNode *node)
{
    if(node == nullptr) { return QString(); }

    QString link = dataPageLink();
    const QStringList names = node->path();
    for(const QString& name : names) {
        link += cwPageSelectionModel::seperator() + kNodePagePrefix + name;
    }
    return link;
}

/**
 * The same address as nodeLink(), for callers still typed cwCave.
 */
QString cwLinkGenerator::caveLink(cwCave *cave)
{
    return nodeLink(cave);
}

/**
 * @brief cwLinkGenerator::fixStationsLink
 * @param node
 * @return The address of the node's Fix Stations sub-page.
 *
 * NodePage.qml registers that sub-page only once the node page itself is
 * current, so this address names a page that may not exist yet.
 * cwPageSelectionModel::setCurrentPageAddress() handles that: it walks the
 * parent addresses first, and visiting the node page is what registers the
 * sub-page the last step then finds.
 */
QString cwLinkGenerator::fixStationsLink(cwSurveyNode *node)
{
    if(node == nullptr) { return QString(); }
    return nodeLink(node)
           + cwPageSelectionModel::seperator()
           + kFixStationsPageName;
}

/**
 * The address of \a trip's page: its node's address, then Trip=.
 */
QString cwLinkGenerator::tripLink(cwTrip *trip)
{
    if(trip == nullptr) { return QString(); }
    return nodeLink(trip->parentNode())
           + cwPageSelectionModel::seperator()
           + kTripPagePrefix
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

