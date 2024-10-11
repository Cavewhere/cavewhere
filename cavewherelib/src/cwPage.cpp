/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwPage.h"
#include "cwDebug.h"
#include "cwPageSelectionModel.h"

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
}

cwPage::~cwPage()
{
    if(ParentPage != nullptr) {
        Q_ASSERT(ParentPage->ChildPages.contains(Name));
        Q_ASSERT(ParentPage->ChildPages.value(Name) == this);
        ParentPage->ChildPages.remove(Name);
    }

    foreach(cwPage* childPage, ChildPages) {
        childPage->ParentPage = nullptr;
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
    result.reserve( names.size() );
    std::reverse_copy( names.begin(), names.end(), std::back_inserter( result ) );

    return result.join(cwPageSelectionModel::seperator());
}

/**
 * @brief cwPage::removeChild
 * @param page - This removes the child page
 *
 * The caller is responisble for deleting the page. This page isn't no long page's parent
 */
void cwPage::removeChild(cwPage *page)
{
    Q_ASSERT(page->parentPage() == this);
    Q_ASSERT(ChildPages.contains(page->name()));
    Q_ASSERT(ChildPages.value(page->name()) == page);
    ChildPages.remove(page->name());
    page->setParent(nullptr);
    page->ParentPage = nullptr;
}

/**
 * @brief cwPage::splitLinkIntoParts
 * @param pageFullname
 * @return pageFullname.split('/')
 *
 * This will splite a page address into it's indivual parts. It splits on "/" and removes the "/"
 */
QStringList cwPage::splitLinkIntoParts(QString pageFullname)
{
    return pageFullname.split(cwPageSelectionModel::seperator());
}

/**
 * @brief cwPage::setNamingFunction
 * @param signalingObject - The object that will emit signal
 * @param signal - The signal that the object will emit
 * @param functionObject - The object that will call the fuction
 * @param function - The function that will be called
 * @param functionParameter - The paramater that will be passed to the function
 *
 * This function is pretty cool! This allows qml to dynamically bind renaming events to the page.
 * The page might be rename by the user, and the page needs to be update with a new name.
 *
 * For example:
 *
 * The user renames a cave from "Stompbottom" to "SauceBottom"
 *
 * When creating this page in qml, we can connect the cave name's change dynamically.
 *
 * page.setNamingFunction(cave,
 *                        "nameChanged()" //Data signal
 *                        qmlObject
 *                        "functionInQMLObject"
 *                        parametersToFunction)
 *
 * So when nameChanged is emitted from cave, the page will call the functionObject's function with
 * functionParameters. This make the functionObject have full control, how the page is named. This
 * is much clear than using a Connections {} because inside of Instatiator{} the connection can
 * be destoried even though the page and cave still exists. So in that case, the cave could change
 * it's name and the page wouldn't update. Using this function get's round the Connection {},
 * Instatiator{} problem.
 */
void cwPage::setNamingFunction(QObject *object,
                               const QByteArray &signal,
                               QObject *functionObject,
                               const QByteArray &function,
                               QVariant functionParameter)
{
    Q_ASSERT(object != nullptr);

    int signalIndex = object->metaObject()->indexOfSignal(signal.data());
    Q_ASSERT(signalIndex != -1); //If this fails, the signal doesn't exist

    int renamePageSlot = metaObject()->indexOfMethod("renamePage()");
    Q_ASSERT(renamePageSlot != -1);

    QMetaObject::connect(object, signalIndex, this, renamePageSlot);

    RenamingObject = functionObject;
    RenamingFunction = function;
    RenamingeFunctionParameter = functionParameter;
}

/**
 * @brief cwPage::renamePage
 *
 * Using the parameters set in setNamingFunction, this will attempt to rename the link.
 */
void cwPage::renamePage()
{
    if(!RenamingObject.isNull()) {
        QVariant valueReturned;

        bool ran = QMetaObject::invokeMethod(RenamingObject,
                                             RenamingFunction.data(),
                                             Q_RETURN_ARG(QVariant, valueReturned),
                                             Q_ARG(QVariant, RenamingeFunctionParameter));

        if(ran) {
            QString newName = valueReturned.toString();
            setName(newName);
        }
    }
}

/**
* @brief cwPage::setSelectionProperties
* @param selectionProperties
*/
void cwPage::setSelectionProperties(QVariantMap selectionProperties) {
    if(SelectionProperties != selectionProperties) {
        SelectionProperties = selectionProperties;
        emit selectionPropertiesChanged();
    }
}
