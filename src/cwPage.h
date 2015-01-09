/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWPAGE_H
#define CWPAGE_H

//Qt includes
#include <QObject>
#include <QQmlComponent>
#include <QPointer>

class cwPage : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)

public:
    cwPage();
    cwPage(cwPage* parentPage,
         QQmlComponent* pageComponent,
         QString name,
         QVariantMap pageProperties
         );
    ~cwPage();

    cwPage* parentPage() const { return ParentPage; }
    QQmlComponent* component() const { return Component; }

    void setName(QString name);
    QString name() const { return Name; }

    QString fullname() const;

    QVariantMap pageProperties() const { return PageProperties; }

    cwPage* childPage(QString name) const { return ChildPages.value(name, nullptr); }

    static QStringList splitLinkIntoParts(QString pageFullname);

signals:
    void nameChanged();

private:
    cwPage* ParentPage;
    QPointer<QQmlComponent> Component;
    QString Name;
    QVariantMap PageProperties; //To setup properties on the page
    QHash<QString, cwPage*> ChildPages;
};

#endif // CWPAGE_H
