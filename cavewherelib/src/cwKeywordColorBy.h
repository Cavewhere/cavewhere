/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWKEYWORDCOLORBY_H
#define CWKEYWORDCOLORBY_H

// Our includes
#include "cwGlobals.h"
#include "cwColorByUniform.h"

// Qt includes
#include <QObject>
#include <QStringList>

class cwKeywordGroupByKeyModel;
class QAbstractItemModel;

// Categorical color-by: groups the keyword items by one key and hands each
// item's cwVisibilityProxy the slot of its value. Slot i is the i-th value in
// sorted order across the visible items, so a filter that leaves eight values
// gives each its own color. Hidden items keep their last slot until they
// reappear. Items lacking the key, or carrying more than one value for it,
// take kOtherSlot.
class CAVEWHERE_LIB_EXPORT cwKeywordColorBy : public QObject
{
    Q_OBJECT

public:
    // The last ramp stop; cwColorRamp::bake fills it with the categorical
    // ramp's neutral gray.
    static constexpr int kOtherSlot = cwColorByUniform::kRampStops - 1;

    explicit cwKeywordColorBy(QObject* parent = nullptr);

    // The keyword items to group, typically the keyword filter's accepted
    // model. Any model exposing cwKeywordItemModel's roles works.
    void setItemModel(QAbstractItemModel* model);

    // An empty key assigns nothing; the proxies keep their last value, which
    // the shader ignores outside keyword mode.
    QString key() const;
    void setKey(const QString& key);

    // The values that own a slot, in slot order, then the "Others" label when
    // some item lacks the key. Feeds the legend.
    QStringList values() const { return m_values; }

    // Number of values that own a slot ("Others" excluded).
    int slotCount() const { return m_slotCount; }

signals:
    // values() or slotCount() changed.
    void slotsChanged();

private:
    void assignSlots();
    void updateValues(bool hasOthers);

    cwKeywordGroupByKeyModel* m_groups;
    // Collapses a burst of group-model signals (a project load inserts items
    // one by one) into one walk on the next event-loop turn.
    bool m_assignPending = false;
    QStringList m_values;
    int m_slotCount = 0;
};

#endif // CWKEYWORDCOLORBY_H
