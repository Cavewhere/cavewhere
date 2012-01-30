import QtQuick 1.1

CoreClickTextInput {

    Pallete {
        id: pallete
    }

    color: readOnly ? pallete.normalTextColor : pallete.inputTextColor
}
