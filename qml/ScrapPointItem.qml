/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 1.0 as QQ // to target S60 5th Edition or Maemo 5
import QtQuick 2.0 as QQ
import Cavewhere 1.0

PointItem {
    id: pointItem

    property Scrap scrap;
    property ScrapItem scrapItem;

    function select() {
        pointItem.selected = true
        scrapItem.selected = true
    }

    focus: selected
    visible: scrapItem === null ? false : scrapItem.selected

    onSelectedChanged: {
        if(selected) {
            scrapItem.selected = true
            forceActiveFocus();
        }
    }
}
