var Buttons = new Array();
var CurrentButton;

function addButton(var button) {
    var buttonIndex = Buttons.size();
    Buttons.push(button);
    button.buttonIndex = buttonIndex;
}

function troggledChanged(var selectedButton) {
    for each (var button in Buttons) {
        if(button != selectedButton) {
            button.troggled = false;
        }
    }
}







