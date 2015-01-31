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
#include <QVariantMap>

//Our inculdes
class cwPage;

/**
 * @brief The cwPageViewAttachedType class
 *
 * This attached item is added to any child of the cwPageView. This attaches the PageView.page
 * property (readonly) to the item in the qml scene.
 *
 * See the Qt documentation on extending c++ and qml and attached properties for more implemenation
 * details.
 *
 * This attached properties are usually used cwPageSelectionModel.registerPage() function to query
 * the page on the current qml item.
 */
class cwPageViewAttachedType : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwPage* page READ page NOTIFY pageChanged)
    Q_PROPERTY(QVariantMap defaultProperties READ defaultProperties WRITE setDefaultProperties NOTIFY defaultPropertiesChanged)
    Q_PROPERTY(QVariantMap defaultSelectionProperties READ defaultSelectionProperties WRITE setDefaultSelectionProperties NOTIFY defaultSelectionPropertiesChanged)

public:
    explicit cwPageViewAttachedType(QObject *parent = 0);
    ~cwPageViewAttachedType();

    cwPage* page() const;
    void setPage(cwPage* page);

    QVariantMap defaultProperties() const;
    void setDefaultProperties(QVariantMap defaultProperties);

    QVariantMap defaultSelectionProperties() const;
    void setDefaultSelectionProperties(QVariantMap defaultSelectionProperties);

signals:
    void pageChanged();
    void defaultPropertiesChanged();
    void defaultSelectionPropertiesChanged();

public slots:

private:
    QPointer<cwPage> Page; //!<
    QVariantMap DefaultProperties; //!<
    QVariantMap DefaultSelectionProperties; //!<

};

/**
* @brief cwPageViewAttachedType::defaultProperties
* @return
*/
inline QVariantMap cwPageViewAttachedType::defaultProperties() const {
    return DefaultProperties;
}

/**
* @brief cwPageViewAttachedType::defaultSelectionProperties
* @return
*/
inline QVariantMap cwPageViewAttachedType::defaultSelectionProperties() const {
    return DefaultSelectionProperties;
}

#endif // CWPAGEVIEWATTACHEDTYPE_H
