/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLABEL3DVIEW_H
#define CWLABEL3DVIEW_H

//Qt includes
#include <QQuickItem>
#include <QMatrix4x4>
#include <QSizeF>
#include <QQmlEngine>
#include <QFont>
#include <QColor>

//Our includes
#include "cwLabel3dItem.h"
#include "cwCollisionRectGrid.h"
#include "cwBillboardOverlayItem.h"
class cwLabel3dGroup;

class CAVEWHERE_LIB_EXPORT cwLabel3dView : public cwBillboardOverlayItem
{
    friend class cwLabel3dGroup;

    Q_OBJECT
    QML_NAMED_ELEMENT(Label3dView)

    // The station label style, forwarded to the shared text layer. The layer
    // holds one style per scene, so the last label view to set it wins; this is
    // consistent only while every label view in a scene uses the same style.
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor outlineColor READ outlineColor WRITE setOutlineColor NOTIFY outlineColorChanged)

public:
    explicit cwLabel3dView(QQuickItem *parent = 0);
    ~cwLabel3dView();

    void addGroup(cwLabel3dGroup* group);
    void removeGroup(cwLabel3dGroup* group);

    QFont font() const;
    void setFont(const QFont& font);

    QColor color() const;
    void setColor(const QColor& color);

    QColor outlineColor() const;
    void setOutlineColor(const QColor& outlineColor);

signals:
    void fontChanged();
    void colorChanged();
    void outlineColorChanged();

public slots:

protected:
    void repositionBillboards() override;
    void claimDeclutterSpace(cwCollisionRectGrid& grid) override;
    QSize declutterCellSize() const override;

private:
    QSet<cwLabel3dGroup*> m_labelGroups;

    cwCollisionRectGrid m_labelGrid; //!< used only outside the scene's declutter pass
    QSizeF m_averageLabelSize = QSizeF(80.0, 14.0);

    QFont m_font;
    QColor m_color = Qt::white;
    QColor m_outlineColor = Qt::black;

    void updateGroup(cwLabel3dGroup* group);
    void updateGroupPositions(cwLabel3dGroup* group, cwCollisionRectGrid& grid);
    cwRenderTextBillboards* textLayer();
    bool applyStyle();
    void restyle();
    void releaseBillboards();
private slots:
    void updatePositions();

};



#endif // CWLABEL3DVIEW_H
