import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
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

        SmithChart {
            Layout.fillWidth: true
            Layout.fillHeight: true
            points: AnalyzerController.points
            z0: AnalyzerController.z0
        }
    }
}
