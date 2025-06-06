/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBaseNoteLeadInteraction.h"
#include "cwScrapItem.h"
#include "cwScrap.h"
#include "cwLead.h"
#include "cwScrapLeadView.h"

cwBaseNoteLeadInteraction::cwBaseNoteLeadInteraction(QQuickItem *parent) :
    cwBaseNotePointInteraction(parent)
{

}

cwBaseNoteLeadInteraction::~cwBaseNoteLeadInteraction()
{

}

/**
 * @brief cwBaseNoteLeadInteraction::addPoint
 * @param notePosition
 * @param scrap
 */
void cwBaseNoteLeadInteraction::addPoint(QPointF notePosition, cwScrapItem *scrapItem)
{
    cwScrap* scrap = scrapItem->scrap();

    cwLead lead;
    lead.setPositionOnNote(notePosition);

    scrap->addLead(lead);

    //Get the last lead in the list and select id
    scrapItem->leadView()->setSelectedItemIndex(scrap->leads().size() - 1);
}

