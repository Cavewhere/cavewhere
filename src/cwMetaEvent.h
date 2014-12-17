/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWMETAEVENT_H
#define CWMETAEVENT_H

#include <QInputEvent>

/**
 * @brief The cwMetaEvent class
 *
 * This class is nessesary on handling menu events on mac.
 */
class cwMetaEvent : public QInputEvent
{
public:
    explicit cwMetaEvent();

signals:

public slots:

};

#endif // CWMETAEVENT_H
