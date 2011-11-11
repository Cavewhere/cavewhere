import QtQuick 1.0

Item {
   id: labelValueUnit

   Row {
       LabelWithHelp {
           id: labelId
           helpArea: helpAreaId
       }


   }

   HelpArea {
       id: helpAreaId
   }
}
