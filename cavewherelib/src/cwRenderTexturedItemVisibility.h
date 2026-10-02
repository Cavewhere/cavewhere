#ifndef CWRENDERTEXTUREDITEMVISIBILITY_H
#define CWRENDERTEXTUREDITEMVISIBILITY_H

#include <QObject>
#include <QPointer>
#include "cwGlobals.h"
#include "cwVisibilityProxy.h"

class cwRenderTexturedItems;

class CAVEWHERE_LIB_EXPORT cwRenderTexturedItemVisibility : public cwVisibilityProxy
{
    Q_OBJECT

public:
    cwRenderTexturedItemVisibility(cwRenderTexturedItems* items,
                                   uint32_t itemId,
                                   QObject* parent = nullptr);

protected:
    void applyVisible(bool visible) override;

private:
    QPointer<cwRenderTexturedItems> m_items;
    uint32_t m_itemId = 0;
};

#endif // CWRENDERTEXTUREDITEMVISIBILITY_H
