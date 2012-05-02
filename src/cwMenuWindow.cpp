//Our includes
#include "cwMenuWindow.h"

//Qt includes
#include <QDebug>

cwMenuWindow::cwMenuWindow(QWindow *parent) : QQuickCanvas(parent)
{
    setWindowFlags(Qt::Popup);
}

/**
 * @brief cwMenuWindow::event
 * @param event
*/
bool cwMenuWindow::event(QEvent *event) {
    bool handled = QQuickCanvas::event(event);

    qDebug() << "Event:" << event;
    if(event->type() == QEvent::FocusOut) {
        handled = true;
        hide();
    }

    return handled;
}

