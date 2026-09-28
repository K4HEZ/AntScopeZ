import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            visible: AnalyzerController.connected
            Label {
                text: AnalyzerController.deviceName
                font.pixelSize: 18
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Disconnect")
                onClicked: AnalyzerController.disconnectAnalyzer()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: !AnalyzerController.connected
            Label {
                text: qsTr("Bluetooth analyzers")
                font.bold: true
                Layout.fillWidth: true
            }
            BusyIndicator {
                running: AnalyzerController.searching
                visible: running
                implicitWidth: 32
                implicitHeight: 32
            }
            Button {
                text: qsTr("Search")
                enabled: !AnalyzerController.searching
                onClicked: AnalyzerController.search()
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !AnalyzerController.connected
            clip: true
            model: AnalyzerController.devices
            delegate: ItemDelegate {
                width: ListView.view.width
                text: modelData
                onClicked: AnalyzerController.connectTo(index)
            }
            Label {
                anchors.centerIn: parent
                visible: parent.count === 0 && !AnalyzerController.searching
                text: qsTr("No analyzers found. Check that the analyzer's Bluetooth is on, then tap Search again.")
                opacity: 0.7
                wrapMode: Text.WordWrap
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Item {
            Layout.fillHeight: true
            visible: AnalyzerController.connected
        }
    }
}
