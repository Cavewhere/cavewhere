/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWINTERACTION_H
#define CWINTERACTION_H

#include <QQuickItem>
#include <QQmlEngine>

/**
    This is the base class for all interaction

    The interaction hold a pointer to a interaction manager.
  */
class cwInteraction : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Interaction)

public:
    explicit cwInteraction(QQuickItem *parent = 0);

public slots:
    void activate();
    void deactivate();

signals:
    void activated(cwInteraction* interaction);
    void deactivated(cwInteraction* interaction);

public slots:


private:

};



#endif // CWINTERACTION_H
