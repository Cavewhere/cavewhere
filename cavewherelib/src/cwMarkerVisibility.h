/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWMARKERVISIBILITY_H
#define CWMARKERVISIBILITY_H

//Qt includes
#include <QPointer>
#include <QString>

//Our includes
#include "cwGlobals.h"
#include "cwVisibilityProxy.h"
class cwMarkerView;

/**
 * \brief The setVisible() target of one marker visibility group's keyword item.
 *
 * cwKeywordVisibility calls setVisible() on this object when keyword filters
 * change. It forwards to cwMarkerView::setGroupKeywordVisible(), which shows or
 * hides every marker of the group. It holds the group name, so it stays valid
 * while the source edits its markers.
 */
class CAVEWHERE_LIB_EXPORT cwMarkerVisibility : public cwVisibilityProxy
{
    Q_OBJECT

public:
    cwMarkerVisibility(cwMarkerView* view, const QString& group, QObject* parent = nullptr);

protected:
    void applyVisible(bool visible) override;

private:
    QPointer<cwMarkerView> m_view;
    QString m_group;
};

#endif // CWMARKERVISIBILITY_H
