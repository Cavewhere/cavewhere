/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSELECTIONMANAGER_H
#define CWSELECTIONMANAGER_H

//Qt includes
#include <QObject>
#include <QtQmlIntegration>
class QQuickItem;

class cwSelectionManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SelectionManager)

    Q_PROPERTY(QQuickItem* selectedItem READ selectedItem WRITE setSelectedItem NOTIFY selectedItemChanged)

public:
    explicit cwSelectionManager(QObject *parent = 0);

    QQuickItem* selectedItem() const;
    void setSelectedItem(QQuickItem* selectedItem);

    Q_INVOKABLE void cycleSelectItem(QQuickItem* item, ulong timestamp);

    bool isSelected(QQuickItem* item) const;
    void clear();

signals:
    void selectedItemChanged();

public slots:

private slots:
    void selectedItemDestroyed();

private:
    QQuickItem* SelectedItem; //!< The currently selected item

    bool m_cylceUpdateQueued = false;
    ulong m_cycleTimestamp = 0; //Unique id that we're using from QEventPoint
    QList<QQuickItem*> m_cycleSelects;
    QList<QQuickItem*> m_cycleQueue;
};

/**
Gets selectedItem
*/
inline QQuickItem* cwSelectionManager::selectedItem() const {
    return SelectedItem;
}

/**
 * @brief cwSelectionManager::isSelected
 * @param item - The item that's
 * @return Returns true, if item is currently selected
 */
inline bool cwSelectionManager::isSelected(QQuickItem *item) const
{
    return SelectedItem == item;
}

/**
 * @brief cwSelectionManager::clear
 *
 * Clears the current selection
 */
inline void cwSelectionManager::clear()
{
    setSelectedItem(nullptr);
}


#endif // CWSELECTIONMANAGER_H
