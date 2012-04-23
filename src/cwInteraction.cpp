#include "cwInteraction.h"

cwInteraction::cwInteraction(QQuickItem *parent) :
    QQuickItem(parent)
{

}

void cwInteraction::activate()
{
    emit activated(this);
}

void cwInteraction::deactivate()
{
    emit deactivated(this);
}

