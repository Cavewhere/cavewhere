/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEDLSETTINGS_H
#define CWEDLSETTINGS_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

//Our includes
#include "cwEdlParametersData.h"
#include "CaveWhereLibExport.h"

/**
 * @brief Live Eye-Dome Lighting tuning, owned by cwScene.
 *
 * Keeps the effect-specific knobs off the general scene-graph root. The values
 * are pulled by cwRhiScene::synchroize() through parameters() and pushed to the
 * render-thread cwEDLEffect; changed() lets cwScene schedule a repaint. See
 * cwEDLEffect / EDL.frag.
 */
class CAVEWHERE_LIB_EXPORT cwEDLSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(EDLSettings)
    QML_UNCREATABLE("EDLSettings is owned by Scene and can't be created directly")

    Q_PROPERTY(float strength READ strength WRITE setStrength NOTIFY strengthChanged)
    Q_PROPERTY(float maxDarken READ maxDarken WRITE setMaxDarken NOTIFY maxDarkenChanged)
    Q_PROPERTY(float radius READ radius WRITE setRadius NOTIFY radiusChanged)

public:
    explicit cwEDLSettings(QObject* parent = nullptr);

    float strength() const { return m_data.strength; }
    void setStrength(float strength);

    float maxDarken() const { return m_data.maxDarken; }
    void setMaxDarken(float maxDarken);

    float radius() const { return m_data.radiusPx; }
    void setRadius(float radius);

    EdlParametersData parameters() const { return m_data; }

signals:
    void strengthChanged();
    void maxDarkenChanged();
    void radiusChanged();

    // Any parameter changed — cwScene listens to schedule a repaint.
    void changed();

private:
    EdlParametersData m_data;
};

#endif // CWEDLSETTINGS_H
