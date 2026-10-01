import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    function refreshLimits() {
        absMinField.text = String(Math.round(AnalyzerController.absMinKHz))
        absMaxField.text = String(Math.round(AnalyzerController.absMaxKHz))
    }

    Connections {
        target: AnalyzerController
        function onLimitsChanged() { refreshLimits() }
    }

    Component.onCompleted: refreshLimits()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        Label { text: qsTr("Add margin to scans"); font.bold: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Slider {
                from: 0
                to: 100
                stepSize: 1
                snapMode: Slider.SnapAlways
                value: BandPresets.widenPercent
                onMoved: BandPresets.widenPercent = Math.round(value)
                Layout.fillWidth: true
            }
            Label {
                text: BandPresets.widenPercent + " %"
                Layout.preferredWidth: 48
                horizontalAlignment: Text.AlignRight
            }
        }

        Label { text: qsTr("SWR chart density"); font.bold: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
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

        Label { text: qsTr("Distance units"); font.bold: true }

        Switch {
            text: AnalyzerController.metricUnits ? qsTr("Metric (m)") : qsTr("Feet (ft)")
            checked: AnalyzerController.metricUnits
            onToggled: AnalyzerController.metricUnits = checked
        }

        Label { text: qsTr("Frequency limits"); font.bold: true }

        Switch {
            text: qsTr("Use device range")
            checked: AnalyzerController.useDeviceRange
            onToggled: AnalyzerController.useDeviceRange = checked
        }

        GridLayout {
            columns: 2
            columnSpacing: 10
            rowSpacing: 6
            Layout.fillWidth: true

            Label { text: qsTr("Absolute min, kHz") }
            Label { text: qsTr("Absolute max, kHz") }
            TextField {
                id: absMinField
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                Layout.fillWidth: true
                onEditingFinished: AnalyzerController.absMinKHz = Number(text)
            }
            TextField {
                id: absMaxField
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                Layout.fillWidth: true
                onEditingFinished: AnalyzerController.absMaxKHz = Number(text)
            }
        }

        Item { Layout.fillHeight: true }
    }
}
