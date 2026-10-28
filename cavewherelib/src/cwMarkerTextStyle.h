/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWMARKERTEXTSTYLE_H
#define CWMARKERTEXTSTYLE_H

//Qt includes
#include <QColor>
#include <QFont>
#include <QObject>
#include <QQmlEngine>
#include <QString>

//Our includes
#include "CaveWhereLibExport.h"

/**
 * @brief How a cwMarkerView draws the marker text lines of one style key.
 *
 * The lines whose cwMarkerTextLine::styleKey is styleKey draw in the scene's
 * text layer of layerKey, styled with font, color and outlineColor. An empty
 * layerKey is the view's own layer key. A view fetches its styles' layers in
 * list order, which is their draw order.
 */
class CAVEWHERE_LIB_EXPORT cwMarkerTextStyle : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MarkerTextStyle)

    Q_PROPERTY(QString styleKey READ styleKey WRITE setStyleKey NOTIFY styleKeyChanged)
    Q_PROPERTY(QString layerKey READ layerKey WRITE setLayerKey NOTIFY layerKeyChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor outlineColor READ outlineColor WRITE setOutlineColor NOTIFY outlineColorChanged)

public:
    explicit cwMarkerTextStyle(QObject* parent = nullptr);

    QString styleKey() const;
    void setStyleKey(const QString& styleKey);

    QString layerKey() const;
    void setLayerKey(const QString& layerKey);

    QFont font() const;
    void setFont(const QFont& font);

    QColor color() const;
    void setColor(const QColor& color);

    QColor outlineColor() const;
    void setOutlineColor(const QColor& outlineColor);

signals:
    void styleKeyChanged();
    void layerKeyChanged();
    void fontChanged();
    void colorChanged();
    void outlineColorChanged();

private:
    QString m_styleKey;
    QString m_layerKey;
    QFont m_font;
    QColor m_color = Qt::white;
    QColor m_outlineColor = Qt::black;
};

#endif // CWMARKERTEXTSTYLE_H
