import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    readonly property var best: AnalyzerController.minSwrIndex >= 0
                                ? AnalyzerController.points[AnalyzerController.minSwrIndex] : null

    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentHeight: column.implicitHeight
        clip: true

        ColumnLayout {
            id: column
            width: parent.width
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
                text: best ? qsTr("Min SWR %1 at %2 MHz").arg(Number(best.swr).toFixed(2)).arg(Number(best.fq).toFixed(3)) : ""
                font.pixelSize: 18
                font.bold: true
            }

            SwrChart {
                id: chart
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                points: AnalyzerController.points
                bands: BandPresets.bands
                selectedIndex: AnalyzerController.selectedIndex
                minPixelsPerPoint: AnalyzerController.chartMinPxPerPoint
                onPointSelected: (index) => AnalyzerController.selectedIndex = index
            }

            // Only when the scan is denser than the chart's minimum spacing.
            ScrollBar {
                id: panBar
                orientation: Qt.Horizontal
                Layout.fillWidth: true
                visible: chart.viewSize < 1
                policy: ScrollBar.AlwaysOn
                size: chart.viewSize
                onPositionChanged: if (pressed) chart.viewPosition = position
            }
            Binding {
                target: panBar
                property: "position"
                value: chart.viewPosition
                when: !panBar.pressed
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Label { text: qsTr("Density") }
                Slider {
                    from: 1
                    to: 30
                    stepSize: 0.5
                    snapMode: Slider.SnapAlways
                    value: AnalyzerController.chartMinPxPerPoint
                    onMoved: AnalyzerController.chartMinPxPerPoint = value
                    Layout.fillWidth: true
                }
                Label {
                    text: qsTr("%1 px/pt").arg(AnalyzerController.chartMinPxPerPoint)
                    Layout.preferredWidth: 72
                    horizontalAlignment: Text.AlignRight
                }
            }

            Label {
                text: qsTr("Touch the chart to read a point.")
                opacity: 0.7
            }

            ParametersGrid {
                visible: AnalyzerController.selectedIndex >= 0
                point: AnalyzerController.selectedPoint
                Layout.fillWidth: true
            }
        }
    }
}
