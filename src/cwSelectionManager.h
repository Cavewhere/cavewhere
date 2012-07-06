#ifndef CWSELECTIONMANAGER_H
#define CWSELECTIONMANAGER_H

//Qt includes
#include <QObject>
class QQuickItem;

class cwSelectionManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QQuickItem* selectedItem READ selectedItem WRITE setSelectedItem NOTIFY selectedItemChanged)

public:
    explicit cwSelectionManager(QObject *parent = 0);

    QQuickItem* selectedItem() const;
    void setSelectedItem(QQuickItem* selectedItem);

    bool isSelected(QQuickItem* item) const;
    void clear();

signals:
    void selectedItemChanged();

public slots:

private slots:
    void selectedItemDestroyed();

private:
    QQuickItem* SelectedItem; //!< The currently selected item
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
    setSelectedItem(NULL);
}


#endif // CWSELECTIONMANAGER_H
