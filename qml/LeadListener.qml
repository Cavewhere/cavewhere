import QtQuick 2.0
import Cavewhere 1.0

QtObject {
    id: listener

    property Scrap scrap;
    property int index: -1
    property string description;
    property string width;
    property string height;
    property bool completed;
    property var unitModel;
    property int unit;

    //Private properties
    property Connections _scrapConnection:
        Connections {
        target: scrap === null ? null : scrap
        onLeadsDataChanged: updateData(begin, end, roles);
        ignoreUnknownSignals: true
    }

    function toLeadDim(text) {
        if(text === "?") {
            return -1;
        } else if(text < 0) {
            return -1;
        }
        return text;
    }

    function update() {
        updateData(index,
                   index,
                   [Scrap.LeadSupportedUnits,
                    Scrap.LeadUnits,
                    Scrap.LeadSize,
                    Scrap.LeadDesciption,
                    Scrap.LeadCompleted])
    }

    function updateData(begin, end, roles) {
        if(scrap === null) { return; }
        if(index < 0 || index >= scrap.numberOfLeads()) { return; }
        if(index >= begin && index <= end) {
            for(var roleIndex in roles) {
                var role = roles[roleIndex]
                switch(role) {
                case Scrap.LeadDesciption:
                    description = scrap.leadData(role, index);
                    break;
                case Scrap.LeadSize:
                    var size = scrap.leadData(role, index);
                    width = (size.width >= 0 ? size.width : "?")
                    height = (size.height >= 0 ? size.height : "?")
                    break;
                case Scrap.LeadSupportedUnits:
                    unitModel = scrap.leadData(role, index);
                    break;
                case Scrap.LeadUnits:
                    unit = scrap.leadData(role, index);
                    break;
                case Scrap.LeadCompleted:
                    completed = scrap.leadData(role, index);
                    break;
                default:
                    break;
                }
            }
        }
    }

    onIndexChanged: update()
    onScrapChanged: update()
    onDescriptionChanged: scrap.setLeadData(Scrap.LeadDesciption, index, description)
    onWidthChanged: scrap.setLeadData(Scrap.LeadSize,
                                      index,
                                      Qt.size(width, height))
    onHeightChanged: scrap.setLeadData(Scrap.LeadSize,
                                       index,
                                       Qt.size(width, height))
    onCompletedChanged: scrap.setLeadData(Scrap.LeadCompleted,
                                          index,
                                          completed)






}

