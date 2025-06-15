/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib


PanZoomInteraction {
    id: interactionId

    anchors.fill: parent

    required property ScrapView scrapView
    required property ImageItem imageItem

    QQ.TapHandler {
        enabled: interactionId.enabled
        parent: interactionId.target
        target: interactionId.target
        onTapped: (eventPoint, button) => {
                      interactionId.scrapView.selectScrapAt(eventPoint.position)
                  }
    }

}
