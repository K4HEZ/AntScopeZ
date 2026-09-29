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

        Label {
            text: qsTr("%1 – %2 kHz, %3 points")
                  .arg(AnalyzerController.fromKHz).arg(AnalyzerController.toKHz).arg(AnalyzerController.sweepPoints)
            opacity: 0.7
        }

        Button {
            Layout.fillWidth: true
            text: AnalyzerController.measuring ? qsTr("Stop") : qsTr("Scan")
            enabled: AnalyzerController.connected
            highlighted: true
            onClicked: AnalyzerController.measuring ? AnalyzerController.stop() : AnalyzerController.scan()
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
            bands: BandPresets.bands
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
