import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    readonly property var best: AnalyzerController.minSwrIndex >= 0
                                ? AnalyzerController.points[AnalyzerController.minSwrIndex] : null

    function fmt(v) { return Number(v).toFixed(2) }
    function fmtFq(mhz) { return Number(mhz).toFixed(3) }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        GridLayout {
            columns: 3
            columnSpacing: 10
            Layout.fillWidth: true

            Label { text: qsTr("Start, kHz") }
            Label { text: qsTr("Stop, kHz") }
            Label { text: qsTr("Points") }

            TextField {
                id: fromField
                text: "14000"
                inputMethodHints: Qt.ImhDigitsOnly
                Layout.fillWidth: true
            }
            TextField {
                id: toField
                text: "14350"
                inputMethodHints: Qt.ImhDigitsOnly
                Layout.fillWidth: true
            }
            TextField {
                id: pointsField
                text: "50"
                inputMethodHints: Qt.ImhDigitsOnly
                Layout.preferredWidth: 70
            }
        }

        Button {
            Layout.fillWidth: true
            text: AnalyzerController.measuring ? qsTr("Stop") : qsTr("Scan")
            enabled: AnalyzerController.connected
            highlighted: true
            onClicked: AnalyzerController.measuring
                       ? AnalyzerController.stop()
                       : AnalyzerController.scan(Number(fromField.text), Number(toField.text),
                                                 parseInt(pointsField.text))
        }

        Label {
            visible: best !== null
            text: best ? qsTr("Min SWR %1 at %2 MHz").arg(fmt(best.swr)).arg(fmtFq(best.fq)) : ""
            font.pixelSize: 18
            font.bold: true
        }

        SwrChart {
            Layout.fillWidth: true
            Layout.preferredHeight: 220
            points: AnalyzerController.points
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: AnalyzerController.points
            header: RowLayout {
                width: ListView.view.width
                Label { text: qsTr("MHz"); font.bold: true; Layout.preferredWidth: parent.width * 0.3 }
                Label { text: qsTr("SWR"); font.bold: true; Layout.preferredWidth: parent.width * 0.2 }
                Label { text: qsTr("R + jX, Ω"); font.bold: true; Layout.fillWidth: true }
            }
            delegate: RowLayout {
                width: ListView.view.width
                Label { text: fmtFq(modelData.fq); Layout.preferredWidth: parent.width * 0.3 }
                Label { text: fmt(modelData.swr); Layout.preferredWidth: parent.width * 0.2 }
                Label {
                    text: fmt(modelData.r) + (modelData.x < 0 ? " − j" : " + j") + fmt(Math.abs(modelData.x))
                    Layout.fillWidth: true
                }
            }
        }
    }
}
