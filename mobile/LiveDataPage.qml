import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    // Four flat bands, not a gradient: <2 green, 2-<3 yellow, 3-4 orange, >4 red.
    function swrColor(swr) {
        if (swr < 2)
            return "#5CC98A" // green
        if (swr < 3)
            return "#E8C547" // yellow
        if (swr <= 4)
            return "#F2A33A" // orange
        return "#F26A6A" // red
    }

    Component.onCompleted: fqField.text = ((AnalyzerController.fromKHz + AnalyzerController.toKHz) / 2).toFixed(0)

    // The device does one thing at a time; don't leave a live loop blocking Scan.
    Component.onDestruction: {
        if (AnalyzerController.liveMode)
            AnalyzerController.stop()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label { text: qsTr("Frequency, kHz") }
            TextField {
                id: fqField
                Layout.fillWidth: true
                enabled: !AnalyzerController.liveMode
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }
        }

        Button {
            Layout.fillWidth: true
            text: AnalyzerController.liveMode ? qsTr("Stop") : qsTr("Start")
            enabled: AnalyzerController.connected && (!AnalyzerController.measuring || AnalyzerController.liveMode)
            highlighted: true
            onClicked: AnalyzerController.liveMode
                       ? AnalyzerController.stop()
                       : AnalyzerController.startLive(Number(fqField.text))
        }

        ParametersGrid {
            point: AnalyzerController.livePoint
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("SWR")
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        // Fills the rest of the page; Text.Fit shrinks the number to whatever
        // size the screen allows.
        Label {
            readonly property bool valid: AnalyzerController.livePoint.swr !== undefined
            text: valid ? Number(AnalyzerController.livePoint.swr).toFixed(2) : "--"
            color: valid ? swrColor(AnalyzerController.livePoint.swr) : palette.text
            font.pixelSize: 1000
            font.bold: true
            fontSizeMode: Text.Fit
            minimumPixelSize: 24
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
