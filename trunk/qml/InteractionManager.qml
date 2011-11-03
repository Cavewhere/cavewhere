import QtQuick 1.0

Item {

    property list<Item> interactions
    property Item defaultInteraction

    //This function hides all other interaction and shows the active interaction
    function active(interaction) {

        //            Make sure the interaction exists
        for(var i = 0; i < interactions.length; i++) {
            var item = interactions[i]
            //Make all interaction invisible
            item.visible = false;
        }

            interaction.visible = true;
        }

            Component.onCompleted: {
                active(defaultInteraction)
            }

        }
