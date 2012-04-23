import QtQuick 2.0

CoreClickTextInput {

    Pallete {
        id: pallete
    }

    color: readOnly ? pallete.normalTextColor : pallete.inputTextColor
}
