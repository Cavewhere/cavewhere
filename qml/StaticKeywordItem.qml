import QtQuick 2.0
import Cavewhere 1.0

KeywordItem {
    id: keywordItem

    property string type

    Component.onCompleted: {
        var keyword = keywordModel.createKeyword("Type", type)
        keywordItem.keywordModel.add(keyword);
        rootData.keywordItemModel.addItem(keywordItem)
    }

}
