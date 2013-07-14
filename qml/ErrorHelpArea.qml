/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 2.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0

HelpArea {

    Style {
        id: style
    }

    color: style.errorBackground
    imageSource: "qrc:icons/stopSignError.png"

}
