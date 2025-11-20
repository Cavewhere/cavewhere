#ifndef CWRENDERTEXTUREDITEMVISIBILITY_H
#define CWRENDERTEXTUREDITEMVISIBILITY_H

#include <QObject>
#include <QPointer>
#include "cwGlobals.h"

class cwRenderTexturedItems;

class CAVEWHERE_LIB_EXPORT cwRenderTexturedItemVisibility : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)

public:
    cwRenderTexturedItemVisibility(cwRenderTexturedItems* items,
                                   uint32_t itemId,
                                   QObject* parent = nullptr);

    bool isVisible() const { return m_visible; }

public slots:
    void setVisible(bool visible);

signals:
    void visibleChanged();

private:
    QPointer<cwRenderTexturedItems> m_items;
    uint32_t m_itemId = 0;
    bool m_visible = true;
};

#endif // CWRENDERTEXTUREDITEMVISIBILITY_H
