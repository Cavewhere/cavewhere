/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwPageViewAttachedType.h"
#include "cwPage.h"

cwPageViewAttachedType::cwPageViewAttachedType(QObject *parent) : QObject(parent)
{

}

cwPageViewAttachedType::~cwPageViewAttachedType()
{

}


/**
* @brief cwPageViewAttachedType::page
* @return Returns the cwPage for the attached property
*/
cwPage* cwPageViewAttachedType::page() const {
    return Page;
}

/**
* @brief cwPageViewAttachedType::setPage
* @param page - Sets the current page attached property
*/
void cwPageViewAttachedType::setPage(cwPage* page) {
    if(Page != page) {
        Page = page;
        emit pageChanged();
    }
}

/**
* @brief cwPageViewAttachedType::setDefaultProperties
* @param defaultProperties
*/
void cwPageViewAttachedType::setDefaultProperties(QVariantMap defaultProperties) {
    if(DefaultProperties != defaultProperties) {
        DefaultProperties = defaultProperties;
        emit defaultPropertiesChanged();
    }
}
