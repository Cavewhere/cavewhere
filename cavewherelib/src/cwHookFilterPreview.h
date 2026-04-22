/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWHOOKFILTERPREVIEW_H
#define CWHOOKFILTERPREVIEW_H

//Qt includes
#include <QCanvasPainterItem>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"

// Small live-preview canvas rendered inside the Sketch settings tab. It
// bakes in a real captured pen stroke with a visible head hook and, each
// frame, runs cwPenStrokeFilter::trimHooks with the currently chosen
// maxHookLength / maxHookFraction so the user can see how the tuning
// values affect a real stroke before committing to them.
class CAVEWHERE_LIB_EXPORT cwHookFilterPreview : public QCanvasPainterItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(HookFilterPreview)

    Q_PROPERTY(double maxHookLength READ maxHookLength WRITE setMaxHookLength NOTIFY maxHookLengthChanged)
    Q_PROPERTY(double maxHookFraction READ maxHookFraction WRITE setMaxHookFraction NOTIFY maxHookFractionChanged)
    Q_PROPERTY(bool filtered READ filtered WRITE setFiltered NOTIFY filteredChanged)
    Q_PROPERTY(int sampleIndex READ sampleIndex WRITE setSampleIndex NOTIFY sampleIndexChanged)

public:
    explicit cwHookFilterPreview(QQuickItem *parent = nullptr);

    double maxHookLength() const { return m_maxHookLength; }
    void setMaxHookLength(double meters);

    double maxHookFraction() const { return m_maxHookFraction; }
    void setMaxHookFraction(double fraction);

    bool filtered() const { return m_filtered; }
    void setFiltered(bool on);

    int sampleIndex() const { return m_sampleIndex; }
    void setSampleIndex(int index);

protected:
    QCanvasPainterItemRenderer *createItemRenderer() const override;

signals:
    void maxHookLengthChanged();
    void maxHookFractionChanged();
    void filteredChanged();
    void sampleIndexChanged();

private:
    double m_maxHookLength = 0.050;
    double m_maxHookFraction = 0.15;
    bool m_filtered = false;
    int m_sampleIndex = 0;
};

#endif // CWHOOKFILTERPREVIEW_H
