/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPOSITIONITEM_H
#define CWPOSITIONITEM_H

//Qt includes
#include <QQuickItem>
#include <QVector3D>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"

/**
  \brief The contract for any item positioned by cwTransformUpdater.

  cwTransformUpdater reprojects each managed item's 3D position into Qt view
  coordinates every frame. An item opts into that by being a PositionItem:
  position3D is the model-space point the updater reads, and inFrustum is the
  membership flag the updater writes - false once the point projects outside the
  viewport.

  The updater publishes inFrustum rather than writing visible directly, so the
  item's owner composes it into its own visible binding and keeps its own reasons
  to be hidden. Positive polarity (default true) keeps those bindings an AND
  (`visible: inFrustum && ...`) instead of a negation, and leaves a point that's
  never handed to an updater visible rather than stuck hidden.
  */
class CAVEWHERE_LIB_EXPORT cwPositionItem : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PositionItem)

    Q_PROPERTY(QVector3D position3D READ position3D WRITE setPosition3D NOTIFY position3DChanged)
    Q_PROPERTY(bool inFrustum READ inFrustum WRITE setInFrustum NOTIFY inFrustumChanged)

public:
    explicit cwPositionItem(QQuickItem* parent = nullptr);

    QVector3D position3D() const;
    void setPosition3D(const QVector3D& position3D);

    bool inFrustum() const;
    void setInFrustum(bool inFrustum);

signals:
    void position3DChanged();
    void inFrustumChanged();

private:
    QVector3D m_position3D;
    bool m_inFrustum = true; //!< Visible until a projection proves the point is off-screen
};

inline QVector3D cwPositionItem::position3D() const {
    return m_position3D;
}

inline bool cwPositionItem::inFrustum() const {
    return m_inFrustum;
}

#endif // CWPOSITIONITEM_H
