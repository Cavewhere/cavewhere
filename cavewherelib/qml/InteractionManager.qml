/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

QQ.Item {

    property list<Interaction> interactions: []
    property Interaction defaultInteraction
    property Interaction activeInteraction: null

    //This function hides all other interaction and shows the active interaction
    function active(interaction) {

        //Make sure the interaction exists
        for(let i = 0; i < interactions.length; i++) {
            let item = interactions[i];

            console.log("Disable:" + item)

            //Make all interaction invisible
            item.visible = false;
            item.enabled = false
        }

        if(interaction !== null) {
            interaction.visible = true;
            interaction.enabled = true;
            interaction.forceActiveFocus();
        }
        activeInteraction = interaction
    }

    function activeDefaultInteraction() {
        active(defaultInteraction)
    }

    function add(interaction) {
        if(interactions.indexOf(interaction) >= 0) {
            console.warn("Can't add interaction because it has already been added");
            return;
        }

        interactions.push(interaction);
        connectInteraction(interaction);
    }

    function remove(interaction) {
        var indexToRemove = interactions.indexOf(interaction);
        if(indexToRemove >= 0) {
            if(activeInteraction === interaction) {
                activeDefaultInteraction();
            }

            interaction.activated.disconnect(active);
            interaction.deactivated.disconnect(activeDefaultInteraction)

        }
    }

    function connectInteraction(interaction) {
        interaction.visible = false
        interaction.activated.connect(active);
        interaction.deactivated.connect(activeDefaultInteraction);
    }

    QQ.Component.onCompleted: {
        for(var i = 0; i < interactions.length; i++) {
            var interaction = interactions[i];
            connectInteraction(interaction);
        }

        activeDefaultInteraction()
    }
}
