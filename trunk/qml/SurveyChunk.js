/**
This creates a survey editor view.
*/
//Parent
var Parent;

//Data
var SurveyModel;
//var StationModel;

//Geometry
var StationTitelLabel;
var LeftTitleLabel;
var RightTitleLabel;
var UpTitleLabel;
var DownTitleLabel;
var DistanceTitleLabel;
var ClinoTitleLabel;
var CompassTitleLabel;


//var xPos; //x position, top left
//var yPos; //y position, top left
//var StationWidth;
//var DistanceWidth;
//var AzimuthWidth;
//var ClinoWidth;
//var LeftWidth;
//var RightWidth;
//var UpWidth;
//var DownWidth;
//var ItemHeight; //Height of each one of the elements
//var StationLeftAnchor;
//var LeftLeftAnchor;
//var RightLeftAnchor;
//var UpLeftAnchor;
//var DownLeftAnchor;
//var DistanceAnchor;
//var AzimuthAnchor;
//var ClinoAnchor;


//Station elements
var StationElements = new Array();
var LeftElements = new Array();
var RightElements = new Array();
var UpElements = new Array();
var DownElements = new Array();

var DistanceElements = new Array();
var AzimuthElements = new Array();
var BackAzimuthElements = new Array();
var ClinoElements = new Array();
var BackClinoElements = new Array();




/**
\brief Creates the view from the model
*/
function CreateView() {

    //Clear all the arrays
    var stationBoxComponent = Qt.createComponent("StationBox.qml");
    var distanceComponent = Qt.createComponent("ShotDistanceDataBox.qml");
    var leftComponent = Qt.createComponent("LeftDataBox.qml");
    var rightComponent = Qt.createComponent("RightDataBox.qml");
    var upComponent = Qt.createComponent("UpDataBox.qml");
    var downComponent = Qt.createComponent("DownDataBox.qml");
    var clinoComponent = Qt.createComponent("FrontClinoReadBox.qml");
    var backClinoComponent = Qt.createComponent("BackClinoReadBox.qml");
    var compassComponent = Qt.createComponent("FrontCompassReadBox.qml");
    var backCompassComponent = Qt.createComponent("BackCompassReadBox.qml");
    var dataBoxComponent = Qt.createComponent("DataBox.qml");

    //Create all the shot elements
    for(var i = 0; i < SurveyModel.ShotCount(); i++) {
        DistanceElements[i] = distanceComponent.createObject(Parent);
        AzimuthElements[i] = compassComponent.createObject(Parent);
        BackAzimuthElements[i] = backCompassComponent.createObject(Parent);
        ClinoElements[i] = clinoComponent.createObject(Parent);
        BackClinoElements[i] = backClinoComponent.createObject(Parent);

        SetShotGeometry(i);
        SetShotData(i);
    }

    //Create all the station elements
    for(var i = 0; i < SurveyModel.StationCount(); i++) {
        StationElements[i] = stationBoxComponent.createObject(Parent);
        LeftElements[i] = leftComponent.createObject(Parent);
        RightElements[i] = rightComponent.createObject(Parent);
        UpElements[i] = upComponent.createObject(Parent);
        DownElements[i] = downComponent.createObject(Parent);

        SetStationGeometry(i);
        SetStationData(i);
    }


    UpdateTabbingOrder();
    UpdateNavigationOrder();
    UpdateElementFocus();

    //Updates the parent's size
   // Parent.height = SurveyModel.StationCount() * StationTitelLabel.height + StationTitelLabel.height + 1;
    Parent.width = StationTitelLabel.width + DistanceTitleLabel.width + CompassTitleLabel.width + ClinoTitleLabel.width + LeftTitleLabel.width + RightTitleLabel.width + UpTitleLabel.width + DownTitleLabel.width + 1;


}


/**
Sets the geometry for a station row
*///        chunkGroup.dataChanged.connect(reload);

// console.log("height:" + height + " width:" + lastChunk.width);

 //page.height = height;
function SetStationGeometry(index) {

    var stationAnchorObject;
    var leftAnchorObject;
    var rightAnchorObject;
    var upAnchorObject;
    var downAnchorObject;


    if(index == 0) {
        stationAnchorObject = StationTitelLabel;
        leftAnchorObject = leftTitleLabel;
        rightAnchorObject = rightTitleLabel;
        upAnchorObject = upTitleLabel;
        downAnchorObject = downTitleLabel;
    } else {
        stationAnchorObject = StationElements[index - 1];
        leftAnchorObject = LeftElements[index - 1];
        rightAnchorObject = RightElements[index - 1];
        upAnchorObject = UpElements[index - 1];
        downAnchorObject = DownElements[index - 1];
    }

    SetItemGeometry(StationElements[index], stationAnchorObject);
    SetItemGeometry(LeftElements[index], leftAnchorObject);
    SetItemGeometry(RightElements[index], rightAnchorObject);
    SetItemGeometry(DownElements[index], downAnchorObject);
    SetItemGeometry(UpElements[index], upAnchorObject);
}

function SetStationData(index) {
    var station = SurveyModel.Station(index);

    StationElements[index].dataValue = station.Name;

    station.Left != null ? LeftElements[index].dataValue = station.Left : '';
    station.Right != null ? RightElements[index].dataValue = station.Right : '';
    station.Up != null ? UpElements[index].dataValue = station.Up : '';
    station.Down != null ? DownElements[index].dataValue = station.Down : '';

    StationElements[index].dataObject = station;
    station.NameChanged.connect(StationElements[index].updateView);

    LeftElements[index].dataObject = station;
    station.LeftChanged.connect(LeftElements[index].updateView);

    RightElements[index].dataObject = station;
    station.RightChanged.connect(RightElements[index].updateView);

    UpElements[index].dataObject = station;
    station.UpChanged.connect(UpElements[index].updateView);

    DownElements[index].dataObject = station;
    station.DownChanged.connect(DownElements[index].updateView);
}

function SetShotGeometry(index) {

    var distanceAnchor;
    var azimuthAnchor;
    var clinoAnchor;

    if(index == 0) {
        distanceAnchor = DistanceTitleLabel;
        azimuthAnchor = CompassTitleLabel;
        clinoAnchor = ClinoTitleLabel;
    } else {
        distanceAnchor = DistanceElements[index - 1];
        azimuthAnchor = BackAzimuthElements[index - 1];
        clinoAnchor = BackClinoElements[index - 1];
    }

    SetItemGeometry(DistanceElements[index], distanceAnchor);
    SetItemGeometry(AzimuthElements[index], azimuthAnchor, CompassTitleLabel.height / 2);
    SetItemGeometry(BackAzimuthElements[index], AzimuthElements[index], CompassTitleLabel.height / 2);
    SetItemGeometry(ClinoElements[index], clinoAnchor, ClinoTitleLabel.height / 2);
    SetItemGeometry(BackClinoElements[index], ClinoElements[index], ClinoTitleLabel.height / 2);


//    var halfHeight = ItemHeight * 0.5;
//    var y = yPos + halfHeight + index * ItemHeight;

    //SetItemGeometry()

//    SetItemGeometry(DistanceElements[index], y, DistanceAnchor, DistanceWidth, ItemHeight);
//    SetItemGeometry(AzimuthElements[index], y, AzimuthAnchor, AzimuthWidth, halfHeight);
//    SetItemGeometry(BackAzimuthElements[index], y + halfHeight, AzimuthAnchor, AzimuthWidth, halfHeight);
//    SetItemGeometry(ClinoElements[index], y, ClinoAnchor, ClinoWidth, halfHeight);
//    SetItemGeometry(BackClinoElements[index], y + halfHeight, ClinoAnchor, ClinoWidth, halfHeight);
}

function SetShotData(index) {
    var shot = SurveyModel.Shot(index);

    console.log("shot:" + shot.Distance);

    DistanceElements[index].dataValue = shot.Distance;
    AzimuthElements[index].dataValue = shot.Compass;
    BackAzimuthElements[index].dataValue = shot.BackCompass;
    ClinoElements[index].dataValue = shot.Clino;
    BackClinoElements[index].dataValue = shot.BackClino;

    DistanceElements[index].dataObject = shot;
    shot.DistanceChanged.connect(DistanceElements[index].updateView);

    AzimuthElements[index].dataObject = shot;
    shot.CompassChanged.connect(AzimuthElements[index].updateView);

    BackAzimuthElements[index].dataObject = shot;
    shot.BackCompassChanged.connect(BackAzimuthElements[index].updateView);

    ClinoElements[index].dataObject = shot;
    shot.ClinoChanged.connect(ClinoElements[index].updateView);

    BackClinoElements[index].dataObject = shot;
    shot.BackClinoChanged.connect(BackClinoElements[index].updateView);
}

function SetItemGeometry(item, anchorObject, height) {
    //console.log("item:" + item + " anchorObject:" + anchorObject)

    height = (height == null) ? anchorObject.height : height;

    item.anchors.left = anchorObject.left;
    item.anchors.top = anchorObject.bottom;
    item.height = height;
    item.width = anchorObject.width;
}

/**
\brief Updates the tabbing order for the whole survey editor
*/
function UpdateTabbingOrder() {
    if(SurveyModel.StationCount() >= 2 && SurveyModel.ShotCount() >= 1) {
        //Special case object
        StationElements[0].nextTabObject = StationElements[1];
        StationElements[1].nextTabObject = DistanceElements[0];
        StationElements[1].previousTabObject = StationElements[0];
        ShotTabOrder(0, StationElements[1], LeftElements[0]);
        LRUDTabOrder(0, BackClinoElements[0], LeftElements[1]);

        var nextObject
        if(SurveyModel.StationCount() >= 3) {
            nextObject = StationElements[2];
        }
        LRUDTabOrder(1, DownElements[0], nextObject);
    }

    //The general case
    for(var i = 2; i < SurveyModel.StationCount(); i++) {
        var shotIndex = i - 1;
        StationElements[i].nextTabObject = DistanceElements[shotIndex];
        StationElements[i].previousTabObject = DownElements[i - 1];
        ShotTabOrder(shotIndex, StationElements[i], LeftElements[i]);

        var nextObject = null;
        if(i + 1 < SurveyModel.StationCount()) {
            nextObject = StationElements[i + 1];
        }

        LRUDTabOrder(i, BackClinoElements[shotIndex], nextObject);
    }
}

function ShotTabOrder(index, previousObject, nextObject) {
    DistanceElements[index].nextTabObject = AzimuthElements[index];
    AzimuthElements[index].nextTabObject = BackAzimuthElements[index];
    BackAzimuthElements[index].nextTabObject = ClinoElements[index];
    ClinoElements[index].nextTabObject = BackClinoElements[index];
    BackClinoElements[index].nextTabObject = nextObject;

    DistanceElements[index].previousTabObject = previousObject;
    AzimuthElements[index].previousTabObject = DistanceElements[index];
    BackAzimuthElements[index].previousTabObject = AzimuthElements[index];
    ClinoElements[index].previousTabObject = BackAzimuthElements[index];
    BackClinoElements[index].previousTabObject = ClinoElements[index];
}

function LRUDTabOrder(index, previousObject, nextObject) {
    LeftElements[index].nextTabObject = RightElements[index];
    RightElements[index].nextTabObject = UpElements[index];
    UpElements[index].nextTabObject = DownElements[index];
    DownElements[index].nextTabObject = nextObject;

    LeftElements[index].previousTabObject = previousObject;
    RightElements[index].previousTabObject = LeftElements[index];
    UpElements[index].previousTabObject = RightElements[index];
    DownElements[index].previousTabObject = UpElements[index];
}

function UpdateNavigationOrder() {
    //For stations
    for(var i = 0; i < StationElements.length; i++) {
        SetNavigation(StationElements, i, null, DistanceElements[i]);
        SetNavigation(LeftElements, i, ClinoElements[i], RightElements[i]);
        SetNavigation(RightElements, i, LeftElements[i], UpElements[i]);
        SetNavigation(UpElements, i, RightElements[i], DownElements[i]);
        SetNavigation(DownElements, i, UpElements[i], null);
    }
    var lastStation = StationElements.length - 1;
    StationElements[lastStation].navRightObject = DistanceElements[lastStation - 1];
    LeftElements[lastStation].navLeftObject = BackClinoElements[lastStation - 1];

    //For shots
    for(var i = 0; i < DistanceElements.length; i++) {
        SetNavigation(DistanceElements, i, StationElements[i], AzimuthElements[i]);
        SetReadingBoxNavigation(AzimuthElements, BackAzimuthElements, i, DistanceElements[i], DistanceElements[i], ClinoElements[i], BackClinoElements[i]);
        SetReadingBoxNavigation(ClinoElements, BackClinoElements, i, AzimuthElements[i], BackAzimuthElements[i], LeftElements[i], LeftElements[i+1]);
    }
}

function UpdateElementFocus() {
    var lastStation = StationElements.length - 1;
    StationElements[lastStation].focused.disconnect(SurveyModel.AddNewShot);
}

function SetNavigation(array, index, left, right) {
    //Up
    if(index - 1 >= 0) {
        array[index].navUpObject = array[index - 1];
    }

    //Down
    if(index + 1 < array.length) {
        array[index].navDownObject = array[index + 1];
    }

    array[index].navLeftObject = left;
    array[index].navRightObject = right;
}

function SetReadingBoxNavigation(frontSiteArray, backSiteArray, index, left, backLeft, right, backRight) {
    //Up
    if(index - 1 >= 0) {
        frontSiteArray[index].navUpObject = backSiteArray[index - 1];
    }
    backSiteArray[index].navUpObject = frontSiteArray[index];

    //Down
    frontSiteArray[index].navDownObject = backSiteArray[index];
    if(index + 1 < frontSiteArray.length) {
        backSiteArray[index].navDownObject = frontSiteArray[index + 1];
    }

    //Left and right
    frontSiteArray[index].navLeftObject = left;
    frontSiteArray[index].navRightObject = right;
    backSiteArray[index].navLeftObject = backLeft;
    backSiteArray[index].navRightObject = backRight;
}


