import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page
    property int mode: 0 // 0 Bluetooth, 1 USB, 2 Serial

    function fmtRange() {
        if (AnalyzerController.deviceMaxKHz <= 0)
            return "--"
        return qsTr("%1 – %2 MHz")
               .arg(Number(AnalyzerController.deviceMinKHz / 1000).toFixed(3))
               .arg(Number(AnalyzerController.deviceMaxKHz / 1000).toFixed(3))
    }

    Component.onCompleted: AnalyzerController.searchSerial()

    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentHeight: column.implicitHeight
        clip: true

        ColumnLayout {
            id: column
            width: parent.width
            spacing: 12

            Label {
                text: qsTr("Connected Device")
                font.bold: true
            }

            GridLayout {
                columns: 2
                columnSpacing: 24
                rowSpacing: 6
                Layout.fillWidth: true

                Label { text: qsTr("Name"); font.bold: true }
                Label {
                    text: AnalyzerController.connected ? AnalyzerController.deviceName : qsTr("Not connected")
                    Layout.fillWidth: true
                    elide: Label.ElideRight
                }
                Label { text: qsTr("Model"); font.bold: true }
                Label { text: AnalyzerController.deviceModel || "--" }
                Label { text: qsTr("Serial number"); font.bold: true }
                Label { text: AnalyzerController.deviceSerial || "--" }
                Label { text: qsTr("Firmware"); font.bold: true }
                Label { text: AnalyzerController.deviceFirmware || "--" }
                Label { text: qsTr("License"); font.bold: true }
                Label { text: AnalyzerController.deviceLicense || "--" }
                Label { text: qsTr("Interface"); font.bold: true }
                Label { text: AnalyzerController.deviceInterface || "--" }
                Label { text: qsTr("Protocol"); font.bold: true }
                Label { text: AnalyzerController.deviceProtocol || "--" }
                Label { text: qsTr("Port"); font.bold: true }
                Label { text: AnalyzerController.devicePort || "--" }
                Label { text: qsTr("Range"); font.bold: true }
                Label { text: fmtRange() }
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Connect via")
                font.bold: true
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                enabled: !AnalyzerController.connected
                RadioButton {
                    text: qsTr("Bluetooth")
                    checked: page.mode === 0
                    onClicked: page.mode = 0
                }
                RadioButton {
                    text: qsTr("USB")
                    checked: page.mode === 1
                    onClicked: page.mode = 1
                }
                RadioButton {
                    text: qsTr("Serial")
                    checked: page.mode === 2
                    onClicked: page.mode = 2
                }
            }

            ListView {
                id: deviceList
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                visible: page.mode !== 1
                clip: true
                enabled: !AnalyzerController.connected
                currentIndex: -1
                model: page.mode === 0 ? AnalyzerController.devices : AnalyzerController.serialDevices
                onModelChanged: currentIndex = -1
                delegate: ItemDelegate {
                    width: ListView.view.width
                    text: modelData
                    highlighted: ListView.isCurrentItem
                    onClicked: deviceList.currentIndex = index
                }
                Label {
                    anchors.centerIn: parent
                    visible: parent.count === 0 && !AnalyzerController.searching && !AnalyzerController.connected
                             && (page.mode === 0 || AnalyzerController.serialSearched)
                    text: page.mode === 0
                          ? qsTr("No analyzers found. Check that the analyzer's Bluetooth is on, then tap Search.")
                          : qsTr("No serial analyzers found. Plug in a NanoVNA or RigExpert, then tap Search.")
                    opacity: 0.7
                    wrapMode: Text.WordWrap
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            Label {
                visible: page.mode === 1
                text: AnalyzerController.connectingUsb
                      ? qsTr("Waiting for USB permission -- allow it in the system dialog if one appeared.")
                      : qsTr("Plug in the analyzer's USB cable, then tap Connect.")
                opacity: 0.7
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: qsTr("Search")
                    visible: page.mode !== 1
                    enabled: !AnalyzerController.connected && !AnalyzerController.searching
                    onClicked: page.mode === 0 ? AnalyzerController.search() : AnalyzerController.searchSerial()
                }
                Button {
                    text: AnalyzerController.connectingUsb ? qsTr("Cancel") : qsTr("Connect")
                    highlighted: true
                    enabled: AnalyzerController.connectingUsb
                             || (!AnalyzerController.connected && (page.mode === 1 || deviceList.currentIndex >= 0))
                    onClicked: {
                        if (AnalyzerController.connectingUsb) {
                            AnalyzerController.cancelUsbConnect()
                        } else if (page.mode === 1) {
                            AnalyzerController.connectUsb()
                        } else if (page.mode === 0) {
                            AnalyzerController.connectTo(deviceList.currentIndex)
                        } else {
                            AnalyzerController.connectSerial(deviceList.currentIndex)
                        }
                    }
                }
                Button {
                    text: qsTr("Disconnect")
                    enabled: AnalyzerController.connected
                    onClicked: AnalyzerController.disconnectAnalyzer()
                }
                BusyIndicator {
                    running: AnalyzerController.searching || AnalyzerController.connectingUsb
                    visible: running
                    implicitWidth: 32
                    implicitHeight: 32
                }
            }
        }
    }
}
