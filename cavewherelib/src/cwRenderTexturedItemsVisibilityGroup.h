#ifndef CWRENDERTEXTUREDITEMSVISIBILITYGROUP_H
#define CWRENDERTEXTUREDITEMSVISIBILITYGROUP_H

#include <QObject>
#include <QPointer>
#include <QVector>

#include "cwGlobals.h"
#include "cwVisibilityProxy.h"

class cwRenderTexturedItems;

class CAVEWHERE_LIB_EXPORT cwRenderTexturedItemsVisibilityGroup : public cwVisibilityProxy
{
    Q_OBJECT

public:
    cwRenderTexturedItemsVisibilityGroup(cwRenderTexturedItems* items,
                                         QVector<uint32_t> ids,
                                         QObject* parent = nullptr);

protected:
    void applyVisible(bool visible) override;

private:
    QPointer<cwRenderTexturedItems> m_items;
    QVector<uint32_t> m_ids;
};

#endif // CWRENDERTEXTUREDITEMSVISIBILITYGROUP_H
