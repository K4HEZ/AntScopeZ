import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    function fmt(v) { return Number(v).toFixed(2) }
    function fmtFq(mhz) { return Number(mhz).toFixed(3) }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Button {
            Layout.fillWidth: true
            text: qsTr("Share Touchstone file")
            enabled: AnalyzerController.points.length > 0
            onClicked: AnalyzerController.shareTouchstone()
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
