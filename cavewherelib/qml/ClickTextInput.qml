/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

CoreClickTextInput {

    Pallete {
        id: pallete
    }

    color: readOnly ? pallete.normalTextColor : pallete.inputTextColor
}
