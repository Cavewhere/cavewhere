/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWPAGEVIEWATTACHEDTYPE_H
#define CWPAGEVIEWATTACHEDTYPE_H

//Qt includes
#include <QObject>
#include <QPointer>

//Our inculdes
class cwPage;

class cwPageViewAttachedType : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwPage* page READ page WRITE setPage NOTIFY pageChanged)

public:
    explicit cwPageViewAttachedType(QObject *parent = 0);
    ~cwPageViewAttachedType();

    cwPage* page() const;
    void setPage(cwPage* page);

signals:
    void pageChanged();

public slots:

private:
    QPointer<cwPage> Page; //!<

};

#endif // CWPAGEVIEWATTACHEDTYPE_H
