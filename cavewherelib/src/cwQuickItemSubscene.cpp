#include "cwQuickItemSubscene.h"

//Qt private API — references a QQuickItem's node subtree off the 2D overlay tree.
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquickwindow_p.h>

//Qt includes
#include <QQuickItem>
#include <QQuickWindow>

cwQuickItemSubscene::cwQuickItemSubscene(QQuickItem* content, QQuickWindow* window)
    : m_content(content),
      m_window(window)
{
    if (!m_content || !m_window) {
        return;
    }

    auto* contentPrivate = QQuickItemPrivate::get(m_content);
    contentPrivate->refWindow(m_window);
    contentPrivate->refFromEffectItem(true);
    m_referenced = true;

    m_content->update();
    m_window->update();
}

cwQuickItemSubscene::~cwQuickItemSubscene()
{
    if (!m_referenced || !m_content) {
        return;
    }

    auto* contentPrivate = QQuickItemPrivate::get(m_content);
    contentPrivate->derefFromEffectItem(true);
    if (m_window) {
        contentPrivate->derefWindow();
    }
}

bool cwQuickItemSubscene::nodeReady() const
{
    return rootNodeHandle() != 0;
}

quintptr cwQuickItemSubscene::rootNodeHandle() const
{
    if (!m_content) {
        return 0;
    }
    return reinterpret_cast<quintptr>(QQuickItemPrivate::get(m_content)->rootNode());
}

void cwQuickItemSubscene::forceSync()
{
    if (!m_content || !m_window) {
        return;
    }
    QQuickWindowPrivate::get(m_window)->updateDirtyNode(m_content);
}
