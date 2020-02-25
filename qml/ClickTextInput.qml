/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ

CoreClickTextInput {

    Pallete {
        id: pallete
    }

    color: readOnly ? pallete.normalTextColor : pallete.inputTextColor
}
