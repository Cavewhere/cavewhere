/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLABELDECLUTTER_H
#define CWLABELDECLUTTER_H

//Qt includes
#include <QMatrix4x4>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QRect>

//Std includes
#include <vector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwCollisionRectGrid.h"

class cwBillboardOverlayItem;
class cwCamera;

/**
  \brief A scene's shared screen-space declutter pass.

  Overlays that opt in through cwBillboardOverlayItem::declutterTier claim
  screen space in one collision grid, one pass per camera change, in tier
  order: a lower tier claims first. The first participant to reposition after a
  camera change runs the pass for every participant; later ones find it done.
  */
class CAVEWHERE_LIB_EXPORT cwLabelDeclutter : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LabelDeclutter)
    QML_UNCREATABLE("A Scene owns its LabelDeclutter")

public:
    enum Tier {
        NoTier,             // keeps the overlay's own placement
        EntranceTier,       // entrance pins, then entrance text blocks
        StationLabelTier    // station labels, after every entrance claim
    };
    Q_ENUM(Tier)

    explicit cwLabelDeclutter(QObject* parent = nullptr);

    void addParticipant(cwBillboardOverlayItem* overlay);
    void removeParticipant(cwBillboardOverlayItem* overlay);

    // Runs the pass for @a camera unless it already ran for this camera state
    // and nothing invalidated it since.
    void ensurePass(cwCamera* camera);

    // Marks the last pass stale, so the next ensurePass runs again.
    void invalidate();

    // Invalidates and runs one pass on a later event-loop turn, coalescing
    // every request made before it runs.
    void schedulePass();

private slots:
    void runScheduledPass();

private:
    // Sorted by tier when added; equal tiers keep their joining order.
    std::vector<QPointer<cwBillboardOverlayItem>> m_participants;
    cwCollisionRectGrid m_grid;

    // The camera state the last pass ran for.
    QPointer<cwCamera> m_lastCamera;
    QMatrix4x4 m_lastMatrix;
    QRect m_lastViewport;

    bool m_dirty = true;
    bool m_passPending = false;
    bool m_inPass = false;
};

#endif // CWLABELDECLUTTER_H
