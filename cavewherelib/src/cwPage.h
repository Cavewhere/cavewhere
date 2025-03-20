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
#include <QQmlEngine>

class cwPage : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Page)

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QVariantMap selectionProperties READ selectionProperties WRITE setSelectionProperties NOTIFY selectionPropertiesChanged)

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

    QVariantMap selectionProperties() const;
    void setSelectionProperties(QVariantMap selectionProperties);

    Q_INVOKABLE QString fullname() const;

    QVariantMap pageProperties() const { return PageProperties; }

    Q_INVOKABLE cwPage* childPage(QString name) const { return ChildPages.value(name, nullptr); }

    void removeChild(cwPage* page);

    static QStringList splitLinkIntoParts(QString pageFullname);

    Q_INVOKABLE void setNamingFunction(QObject* object, const QByteArray& signal,
                                       QObject* functionObject, const QByteArray& function,
                                       QVariant functionParameter);

signals:
    void nameChanged();
    void selectionPropertiesChanged();

private slots:
    void renamePage();

private:
    cwPage* ParentPage;
    QPointer<QQmlComponent> Component;
    QString Name;
    QVariantMap PageProperties; //To setup properties on the page
    QVariantMap SelectionProperties; //Allows for changing the selection of the page
    QHash<QString, cwPage*> ChildPages;

    //For automatically renaming the page, these parameters are handed to QMetaObject::invokeMethod
    QPointer<QObject> RenamingObject;
    QByteArray RenamingFunction;
    QVariant RenamingeFunctionParameter;


};

/**
* @brief cwPage::selectionProperties
* @return
*/
inline QVariantMap cwPage::selectionProperties() const {
    return SelectionProperties;
}
#endif // CWPAGE_H
