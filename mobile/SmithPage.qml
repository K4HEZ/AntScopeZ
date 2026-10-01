import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentHeight: column.implicitHeight
        clip: true

        ColumnLayout {
            id: column
            width: parent.width
            spacing: 10

            Button {
                Layout.fillWidth: true
                text: AnalyzerController.measuring ? qsTr("Stop") : qsTr("Scan")
                enabled: AnalyzerController.connected
                highlighted: true
                onClicked: AnalyzerController.measuring ? AnalyzerController.stop() : AnalyzerController.scan()
            }

            SmithChart {
                Layout.fillWidth: true
                Layout.preferredHeight: width
                points: AnalyzerController.points
                z0: AnalyzerController.z0
                selectedIndex: AnalyzerController.selectedIndex
                onPointSelected: (index) => AnalyzerController.selectedIndex = index
            }

            Slider {
                Layout.fillWidth: true
                visible: AnalyzerController.points.length > 1
                from: 0
                to: Math.max(1, AnalyzerController.points.length - 1)
                stepSize: 1
                snapMode: Slider.SnapAlways
                value: AnalyzerController.selectedIndex
                onMoved: AnalyzerController.selectedIndex = Math.round(value)
            }

            ParametersGrid {
                visible: AnalyzerController.selectedIndex >= 0
                point: AnalyzerController.selectedPoint
                Layout.fillWidth: true
            }
        }
    }
}
