// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 1.1
import QtDesktop 0.1

TableView {
    frame: false

    TableColumn {
        role: "name"
        title: "Name"
        width: 120
    }

    TableColumn {
        role: "job"
        title: "Job"
        width: 120
    }
}
