//Our includes
#include "cwApplicationPalette.h"

//Qt includes
#include <QGuiApplication>

namespace {
    constexpr QPalette::ColorRole kDisabledTextRoles[] = {
        QPalette::WindowText, QPalette::Text, QPalette::ButtonText
    };
}

cwApplicationPalette::cwApplicationPalette(QObject* parent) :
    QObject(parent)
{
}

void cwApplicationPalette::setDisabledText(const QColor& color)
{
    if (m_disabledText == color) {
        return;
    }
    m_disabledText = color;
    emit colorsChanged();
    scheduleApply();
}

void cwApplicationPalette::classBegin()
{
}

void cwApplicationPalette::componentComplete()
{
    m_complete = true;
    apply();
}

void cwApplicationPalette::setRole(QPalette::ColorRole role, const QColor& color)
{
    if (m_colors.value(role) == color) {
        return;
    }
    m_colors.insert(role, color);
    emit colorsChanged();
    scheduleApply();
}

//A scheme flip changes every role in one pass of the binding engine; queue a
//single application so the palette is rebuilt once, not twenty times.
void cwApplicationPalette::scheduleApply()
{
    if (!m_complete || m_applyQueued) {
        return;
    }
    m_applyQueued = true;
    QMetaObject::invokeMethod(this, &cwApplicationPalette::apply, Qt::QueuedConnection);
}

void cwApplicationPalette::apply()
{
    m_applyQueued = false;

    QPalette palette = QGuiApplication::palette();
    for (auto it = m_colors.cbegin(); it != m_colors.cend(); ++it) {
        if (it.value().isValid()) {
            palette.setColor(QPalette::All, it.key(), it.value());
        }
    }
    if (m_disabledText.isValid()) {
        for (const QPalette::ColorRole role : kDisabledTextRoles) {
            palette.setColor(QPalette::Disabled, role, m_disabledText);
        }
    }
    QGuiApplication::setPalette(palette);
}
