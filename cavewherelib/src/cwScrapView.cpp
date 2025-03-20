/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScrapView.h"
#include "cwNote.h"
#include "cwDebug.h"
#include "cwScrap.h"
#include "cwScrapItem.h"
#include "cwSelectionManager.h"

//Qt includes
#include <QQmlEngine>
#include <QSGTransformNode>

cwScrapView::cwScrapView(QQuickItem *parent) :
    QQuickItem(parent),
    m_note(nullptr),
    m_selectionManager(new cwSelectionManager(this))
{
    connect(this, &cwScrapView::parentChanged,
            this, [this]()
            {
                for(auto item : std::as_const(m_scrapItems)) {
                    updateParent(item);

                }
            });
}

/**
  Sets the note for the scrap view
  */
void cwScrapView::setNote(cwNote* note) {
    if(m_note != note) {

        if(m_note != nullptr) {
            disconnect(m_note, nullptr, this, nullptr);
        }

        m_note = note;

        if(m_note != nullptr) {
            connect(m_note, &cwNote::insertedScraps, this, &cwScrapView::insertScrapItem);
            connect(m_note, &cwNote::removedScraps, this, &cwScrapView::updateRemovedScraps);
        }

        //This is full reset, update all the scraps
        updateAllScraps();

        emit noteChanged();
    }
}

/**
  \brief Updates all the scrap items that are in the view
  */
void cwScrapView:: insertScrapItem(int begin, int end) {
    if(m_note == nullptr) {
        return;
    }

    if(m_note->scraps().empty()) {
        qDebug() << "This is a bug! There are no scraps" << LOCATION;
        return;
    }

    //make sure that there exactly one less scrap
    if(m_note->scraps().size() - 1 != m_scrapItems.size()) {
        qDebug() << "This is a bug! ItemLookup size mismatch" << LOCATION;
        return;
    }

    Q_ASSERT(begin <= end);
    Q_ASSERT(begin >= 0);
    Q_ASSERT(end >= 0);
    Q_ASSERT(begin < m_note->scraps().size());
    Q_ASSERT(end < m_note->scraps().size());

    //Create a new scrap item
    QQmlContext* context = QQmlEngine::contextForObject(this);

    for(int i = begin; i <= end; i++) {
        cwScrapItem* scrapItem = new cwScrapItem(context, this);
        scrapItem->setScrap(m_note->scraps().last());
        scrapItem->setZoom(m_zoom);
        scrapItem->setTargetItem(m_targetItem);
        updateParent(scrapItem);

        scrapItem->setSelectionManager(m_selectionManager);

        m_scrapItems.insert(i, scrapItem);
    }

    //Select the scrapItem
    if(begin == end) {
        setSelectScrapIndex(m_scrapItems.size() - 1);
    }
}

/**
    Gets the scrapItem at notePoint

    If no scrap is defined at this point nullptr is returned.

    \param notePoint - Normalized note coordinate
  */
QList<cwScrapItem*> cwScrapView::scrapItemsAt(QPointF notePoint) {
    QList<cwScrapItem*> items;
    foreach(cwScrapItem* scrapItem, m_scrapItems) {
        QPolygonF polygon(scrapItem->scrap()->points());
        if(polygon.containsPoint(notePoint, Qt::OddEvenFill)) {
            items.append(scrapItem);
        }
    }

    return items;
}

/**
  \brief Gets the scrap item at the index

  If the inde is out of bounds this return nullptr
  */
cwScrapItem *cwScrapView::scrapItemAt(int index) {
    if(index >= 0 && index < m_scrapItems.size()) {
        return m_scrapItems[index];
    }
    return nullptr;
}

void cwScrapView::setZoom(double zoom)
{
    if(m_zoom != zoom) {
        m_zoom = zoom;
        for(auto item : std::as_const(m_scrapItems)) {
            item->setZoom(zoom);
        }
        emit zoomChanged();
    }
}

QTransform cwScrapView::toImage(cwNote *note)
{
    auto imageSize = note->image().originalSize();
    QTransform transform;
    transform.scale(imageSize.width(), imageSize.height());
    transform.scale(1.0, -1.0);
    transform.translate(0.0, -1.0);
    return transform;
}

QTransform cwScrapView::toNormalized(cwNote *note)
{
    return toImage(note).inverted();
}

QPointF cwScrapView::toNoteCoordinates(QPointF imageCoordinates) const
{
    return cwScrapView::toNormalized(note()).map(imageCoordinates);
}

/**
  \brief Selects the scrap at the imagePoint. ImagePoint is in image coordinate system
  */
void cwScrapView::selectScrapAt(QPointF imagePoint) {

    auto notePoint = toNormalized(m_note).map(imagePoint);

    QList<cwScrapItem*> items = scrapItemsAt(notePoint);
    if(items.isEmpty()) {
        return; //Nothing to select
    }

    //Select the first item
    setSelectedScrapItem(items.first());
}

// /**
//   \brief Sets the transform updater
//   */
// void cwScrapView::setTransformUpdater(cwTransformItemUpdater* updater) {
//     if(m_transformUpdater != updater) {
//         m_transformUpdater = updater;

//         updateAllScraps();

//         emit transformUpdaterChanged();
//     }
// }

/**
  This should be called when the note object has changed.

  This will update all the scrap polygons
  */
void cwScrapView::updateAllScraps() {
    int numberOfScraps;
    if(note() != nullptr) {
        numberOfScraps = note()->scraps().size();
    } else {
        numberOfScraps = 0;
    }

    if(m_scrapItems.size() > numberOfScraps) {
        //Delete unused polygons
        for(int i = m_scrapItems.size() - 1; i >= numberOfScraps; i--) {
            if(m_scrapItems[i] == selectedScrapItem()) {
                setSelectedScrapItem(nullptr);
            }

            m_scrapItems[i]->deleteLater();
            m_scrapItems.removeLast();
        }
    }

    if(m_scrapItems.size() < numberOfScraps) {
        QQmlContext* context = QQmlEngine::contextForObject(this);

        //Add new scrap items
        for(int i = m_scrapItems.size(); i < numberOfScraps; i++) {
            cwScrapItem* item = new cwScrapItem(context, this);
            updateParent(item);
            m_scrapItems.append(item);
        }
    }

    Q_ASSERT(m_scrapItems.size() == numberOfScraps);

    //Update
    for(int i = 0; i < numberOfScraps; i++) {
        cwScrap* scrap = note()->scrap(i);
        cwScrapItem* scrapItem = m_scrapItems[i];
        scrapItem->setScrap(scrap);
        scrapItem->setZoom(m_zoom);
        scrapItem->setSelectionManager(m_selectionManager);
        scrapItem->setTargetItem(targetItem());
        connect(scrapItem, SIGNAL(selectedChanged()), SLOT(updateSelection()), Qt::UniqueConnection);
    }
}

/**
    Sets selectScrapIndex
*/
void cwScrapView::setSelectScrapIndex(int selectScrapIndex) {
    if(m_selectScrapIndex != selectScrapIndex) {
        //If selection index is valid
        if(selectScrapIndex >= m_scrapItems.size()) {
            qDebug() << "Can't select invalid scrap of index" << selectScrapIndex << LOCATION;
            return;
        }

        //Remove the selection from the previous
        cwScrapItem* oldScrapItem = selectedScrapItem();
        if(oldScrapItem != nullptr) {
            oldScrapItem->setSelected(false);
        }

        if(selectScrapIndex >= 0) {
            cwScrapItem* newScrapItem = m_scrapItems.at(selectScrapIndex);
            newScrapItem->setSelected(true);
        }

        m_selectScrapIndex = selectScrapIndex;
        emit selectScrapIndexChanged();

        //Deselect any items that were selected in the scrap
        m_selectionManager->clear();
    }
}

/**
    Gets selectedScrap
*/
cwScrapItem* cwScrapView::selectedScrapItem() const {
    if(selectScrapIndex() >= 0 && selectScrapIndex() < m_scrapItems.size()) {
        //        Q_ASSERT(ScrapItems[selectScrapIndex()]->isSelected());
        return m_scrapItems.at(selectScrapIndex());
    }
    return nullptr;
}

/**
    Selects the scrap by pointer
  */
void cwScrapView::setSelectedScrapItem(cwScrapItem* scrapItem) {
    int index = m_scrapItems.indexOf(scrapItem);
    setSelectScrapIndex(index);
}

/**
    This is a private slot

    Called when a scrapItem has changed there selection, this will update the selection
    model, to guaneteer that it is always up to date.
  */
void cwScrapView::updateSelection() {
    cwScrapItem* scrapItem = qobject_cast<cwScrapItem*>(sender());
    if(scrapItem != nullptr) {
        setSelectedScrapItem(scrapItem);
    }
}

/**
 * @brief cwScrapView::updateRemovedScraps
 * @param begin - The index of the first scrap that was removed
 * @param end - The index of the last scrap that was removed
 */
void cwScrapView::updateRemovedScraps(int begin, int end)
{
    Q_UNUSED(begin);
    Q_UNUSED(end);
    updateAllScraps();
}

void cwScrapView::updateParent(cwScrapItem *item)
{
    item->setParentItem(this);
    item->setParent(this);
}


QQuickItem *cwScrapView::targetItem() const
{
    return m_targetItem;
}

void cwScrapView::setTargetItem(QQuickItem *newTargetItem)
{
    if (m_targetItem == newTargetItem)
        return;
    m_targetItem = newTargetItem;
    for(auto item : m_scrapItems) {
        updateParent(item);
    }
    emit targetItemChanged();
}
