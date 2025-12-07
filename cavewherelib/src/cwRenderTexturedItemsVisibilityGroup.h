#ifndef CWRENDERTEXTUREDITEMSVISIBILITYGROUP_H
#define CWRENDERTEXTUREDITEMSVISIBILITYGROUP_H

#include <QObject>
#include <QPointer>
#include <QVector>

#include "cwGlobals.h"

class cwRenderTexturedItems;

class CAVEWHERE_LIB_EXPORT cwRenderTexturedItemsVisibilityGroup : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)

public:
    cwRenderTexturedItemsVisibilityGroup(cwRenderTexturedItems* items,
                                         QVector<uint32_t> ids,
                                         QObject* parent = nullptr);

    bool isVisible() const { return m_visible; }

public slots:
    void setVisible(bool visible);

signals:
    void visibleChanged();

private:
    QPointer<cwRenderTexturedItems> m_items;
    QVector<uint32_t> m_ids;
    bool m_visible = true;
};

#endif // CWRENDERTEXTUREDITEMSVISIBILITYGROUP_H
