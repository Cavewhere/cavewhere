/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ

CoreClickTextInput {

    Pallete {
        id: pallete
    }

    color: readOnly ? pallete.normalTextColor : pallete.inputTextColor
}
