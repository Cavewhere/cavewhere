#ifndef CWAPPLICATIONPALETTE_H
#define CWAPPLICATIONPALETTE_H

//Qt includes
#include <QColor>
#include <QHash>
#include <QObject>
#include <QPalette>
#include <QQmlEngine>
#include <QQmlParserStatus>

//Our includes
#include "CaveWhereLibExport.h"

/**
 * Applies a set of colors as the application palette.
 *
 * Theme.qml owns one instance and binds each role to a Theme token. Under a
 * custom Qt Quick Controls style the controls take their default palette from
 * the application palette, so this one object themes every window, popup,
 * fallback control, and palette-driven module at once, and follows Theme.dark
 * live. Roles left unset keep the application's current color.
 */
class CAVEWHERE_LIB_EXPORT cwApplicationPalette : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_NAMED_ELEMENT(ApplicationPalette)

    Q_PROPERTY(QColor window READ window WRITE setWindow NOTIFY colorsChanged)
    Q_PROPERTY(QColor windowText READ windowText WRITE setWindowText NOTIFY colorsChanged)
    Q_PROPERTY(QColor base READ base WRITE setBase NOTIFY colorsChanged)
    Q_PROPERTY(QColor alternateBase READ alternateBase WRITE setAlternateBase NOTIFY colorsChanged)
    Q_PROPERTY(QColor text READ text WRITE setText NOTIFY colorsChanged)
    Q_PROPERTY(QColor button READ button WRITE setButton NOTIFY colorsChanged)
    Q_PROPERTY(QColor buttonText READ buttonText WRITE setButtonText NOTIFY colorsChanged)
    Q_PROPERTY(QColor brightText READ brightText WRITE setBrightText NOTIFY colorsChanged)
    Q_PROPERTY(QColor highlight READ highlight WRITE setHighlight NOTIFY colorsChanged)
    Q_PROPERTY(QColor highlightedText READ highlightedText WRITE setHighlightedText NOTIFY colorsChanged)
    Q_PROPERTY(QColor placeholderText READ placeholderText WRITE setPlaceholderText NOTIFY colorsChanged)
    Q_PROPERTY(QColor toolTipBase READ toolTipBase WRITE setToolTipBase NOTIFY colorsChanged)
    Q_PROPERTY(QColor toolTipText READ toolTipText WRITE setToolTipText NOTIFY colorsChanged)
    Q_PROPERTY(QColor link READ link WRITE setLink NOTIFY colorsChanged)
    Q_PROPERTY(QColor accent READ accent WRITE setAccent NOTIFY colorsChanged)
    Q_PROPERTY(QColor light READ light WRITE setLight NOTIFY colorsChanged)
    Q_PROPERTY(QColor midlight READ midlight WRITE setMidlight NOTIFY colorsChanged)
    Q_PROPERTY(QColor mid READ mid WRITE setMid NOTIFY colorsChanged)
    Q_PROPERTY(QColor dark READ dark WRITE setDark NOTIFY colorsChanged)
    Q_PROPERTY(QColor shadow READ shadow WRITE setShadow NOTIFY colorsChanged)

    //! Text color of the Disabled group for windowText, text, and buttonText
    Q_PROPERTY(QColor disabledText READ disabledText WRITE setDisabledText NOTIFY colorsChanged)

public:
    explicit cwApplicationPalette(QObject* parent = nullptr);

    QColor window() const { return m_colors.value(QPalette::Window); }
    void setWindow(const QColor& color) { setRole(QPalette::Window, color); }
    QColor windowText() const { return m_colors.value(QPalette::WindowText); }
    void setWindowText(const QColor& color) { setRole(QPalette::WindowText, color); }
    QColor base() const { return m_colors.value(QPalette::Base); }
    void setBase(const QColor& color) { setRole(QPalette::Base, color); }
    QColor alternateBase() const { return m_colors.value(QPalette::AlternateBase); }
    void setAlternateBase(const QColor& color) { setRole(QPalette::AlternateBase, color); }
    QColor text() const { return m_colors.value(QPalette::Text); }
    void setText(const QColor& color) { setRole(QPalette::Text, color); }
    QColor button() const { return m_colors.value(QPalette::Button); }
    void setButton(const QColor& color) { setRole(QPalette::Button, color); }
    QColor buttonText() const { return m_colors.value(QPalette::ButtonText); }
    void setButtonText(const QColor& color) { setRole(QPalette::ButtonText, color); }
    QColor brightText() const { return m_colors.value(QPalette::BrightText); }
    void setBrightText(const QColor& color) { setRole(QPalette::BrightText, color); }
    QColor highlight() const { return m_colors.value(QPalette::Highlight); }
    void setHighlight(const QColor& color) { setRole(QPalette::Highlight, color); }
    QColor highlightedText() const { return m_colors.value(QPalette::HighlightedText); }
    void setHighlightedText(const QColor& color) { setRole(QPalette::HighlightedText, color); }
    QColor placeholderText() const { return m_colors.value(QPalette::PlaceholderText); }
    void setPlaceholderText(const QColor& color) { setRole(QPalette::PlaceholderText, color); }
    QColor toolTipBase() const { return m_colors.value(QPalette::ToolTipBase); }
    void setToolTipBase(const QColor& color) { setRole(QPalette::ToolTipBase, color); }
    QColor toolTipText() const { return m_colors.value(QPalette::ToolTipText); }
    void setToolTipText(const QColor& color) { setRole(QPalette::ToolTipText, color); }
    QColor link() const { return m_colors.value(QPalette::Link); }
    void setLink(const QColor& color) { setRole(QPalette::Link, color); }
    QColor accent() const { return m_colors.value(QPalette::Accent); }
    void setAccent(const QColor& color) { setRole(QPalette::Accent, color); }
    QColor light() const { return m_colors.value(QPalette::Light); }
    void setLight(const QColor& color) { setRole(QPalette::Light, color); }
    QColor midlight() const { return m_colors.value(QPalette::Midlight); }
    void setMidlight(const QColor& color) { setRole(QPalette::Midlight, color); }
    QColor mid() const { return m_colors.value(QPalette::Mid); }
    void setMid(const QColor& color) { setRole(QPalette::Mid, color); }
    QColor dark() const { return m_colors.value(QPalette::Dark); }
    void setDark(const QColor& color) { setRole(QPalette::Dark, color); }
    QColor shadow() const { return m_colors.value(QPalette::Shadow); }
    void setShadow(const QColor& color) { setRole(QPalette::Shadow, color); }

    QColor disabledText() const { return m_disabledText; }
    void setDisabledText(const QColor& color);

    void classBegin() override;
    void componentComplete() override;

signals:
    void colorsChanged();

private:
    void setRole(QPalette::ColorRole role, const QColor& color);
    void scheduleApply();
    void apply();

    QHash<QPalette::ColorRole, QColor> m_colors;
    QColor m_disabledText;
    bool m_complete = false;
    bool m_applyQueued = false;
};

#endif // CWAPPLICATIONPALETTE_H
