/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwPage.h"

cwPage::cwPage()
    : ParentPage(nullptr)
{

}

/**
 * @brief cwPageSelectionModel::Page::Page
 * @param parentPage
 * @param pageComponent
 * @param name
 * @param pageProperties
 */
cwPage::cwPage(cwPage* parentPage,
               QQmlComponent *pageComponent,
               QString name,
               QVariantMap pageProperties) :
    QObject(parentPage),
    ParentPage(parentPage),
    Component(pageComponent),
    Name(name),
    PageProperties(pageProperties)
{
    Q_ASSERT(parentPage != nullptr);

    Q_ASSERT(!ParentPage->ChildPages.contains(Name)); //enforce that names are unique
    ParentPage->ChildPages.insert(Name, this);

    pageComponent->setParent(this);
}

cwPage::~cwPage()
{
    if(ParentPage != nullptr) {
        Q_ASSERT(ParentPage->ChildPages.contains(Name));
        Q_ASSERT(ParentPage->ChildPages.value(Name) == this);
        ParentPage->ChildPages.remove(Name);
    }
}

/**
 * @brief cwPageSelectionModel::Page::setName
 * @param name
 *
 * Sets the name for the page. This will update the name lookup in the parent
 */
void cwPage::setName(QString name)
{
    if(Name != name) {
        if(ParentPage != nullptr) {
            Q_ASSERT(ParentPage->ChildPages.contains(Name));
            Q_ASSERT(ParentPage->ChildPages.value(Name) == this);
            ParentPage->ChildPages.remove(Name);

            Name = name;

            ParentPage->ChildPages.insert(Name, this);

            emit nameChanged();
        }
    }
}

/**
 * @brief cwPage::fullname
 * @return Returns the full name of the page. You can use this as a string link to this page
 */
QString cwPage::fullname() const
{
    QStringList names;

    const cwPage* current = this;
    while(current != nullptr) {
        if(!current->name().isEmpty()) {
            //Make sure we're not at the root node
            names.append(current->name());
        }
        current = current->parentPage();
    }

    QStringList result;
    result.reserve( names.size() ); // reserve is new in Qt 4.7
    std::reverse_copy( names.begin(), names.end(), std::back_inserter( result ) );

    return result.join("/");
}

QStringList cwPage::splitLinkIntoParts(QString pageFullname)
{
    return pageFullname.split('/');
}
