import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page
    property int trace: 0 // 0 impulse, 1 step, 2 impedance
    property int selIndex: -1
    property var estimate: AnalyzerController.tdrEstimate()
    readonly property var peak: AnalyzerController.tdrPeak
    readonly property var traceValues: trace === 0 ? AnalyzerController.tdrImpulse
                                      : trace === 1 ? AnalyzerController.tdrStep
                                      : AnalyzerController.tdrImpedance
    readonly property double knownLength: Number(knownField.text)
    // Re-evaluated when the peak or velocity factor changes.
    readonly property double calcVf: {
        void(AnalyzerController.tdrPeak)
        void(AnalyzerController.tdrVelocityFactor)
        return AnalyzerController.tdrCalculatedVf(knownLength)
    }
    readonly property string note: {
        void(AnalyzerController.tdrPeak)
        void(AnalyzerController.tdrPoints)
        return AnalyzerController.tdrNote(knownLength)
    }

    function refreshFields() {
        vfField.text = String(AnalyzerController.tdrVelocityFactor)
        topField.text = String(Math.round(AnalyzerController.tdrTopKHz))
        pointsField.text = String(AnalyzerController.tdrPoints)
        betaField.text = String(AnalyzerController.tdrKaiserBeta)
        estimate = AnalyzerController.tdrEstimate()
    }

    function fmt(v, d) { return Number(v).toFixed(d) }

    Connections {
        target: AnalyzerController
        function onTdrSettingsChanged() { page.refreshFields() }
        function onTdrChanged() { page.selIndex = -1 }
    }

    Component.onCompleted: refreshFields()

    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentHeight: column.implicitHeight
        clip: true

        ColumnLayout {
            id: column
            width: parent.width
            spacing: 10

            Label { text: qsTr("Scan setup"); font.bold: true }

            Button {
                Layout.fillWidth: true
                enabled: !AnalyzerController.tdrScanning
                text: AnalyzerController.tdrCableName || qsTr("Select cable preset...")
                onClicked: cablePicker.open()
            }

            GridLayout {
                columns: 2
                columnSpacing: 10
                rowSpacing: 6
                Layout.fillWidth: true

                Label { text: qsTr("Velocity factor") }
                Label { text: qsTr("Top frequency, kHz") }
                TextField {
                    id: vfField
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    Layout.fillWidth: true
                    enabled: !AnalyzerController.tdrScanning
                    onEditingFinished: AnalyzerController.tdrVelocityFactor = Number(text)
                }
                TextField {
                    id: topField
                    inputMethodHints: Qt.ImhDigitsOnly
                    Layout.fillWidth: true
                    enabled: !AnalyzerController.tdrScanning
                    onEditingFinished: AnalyzerController.tdrTopKHz = Number(text)
                }
                Label { text: qsTr("Points (200–1000)") }
                Label { text: "" }
                TextField {
                    id: pointsField
                    inputMethodHints: Qt.ImhDigitsOnly
                    Layout.fillWidth: true
                    enabled: !AnalyzerController.tdrScanning
                    onEditingFinished: AnalyzerController.tdrPoints = parseInt(text)
                }
                Item { Layout.fillWidth: true }
            }

            GridLayout {
                columns: 2
                columnSpacing: 24
                rowSpacing: 4
                Layout.fillWidth: true
                Label { text: qsTr("Unambiguous range"); font.bold: true }
                Label { text: estimate.range > 0 ? fmt(estimate.range, 2) + " " + AnalyzerController.tdrUnit : "--" }
                Label { text: qsTr("Resolution (estimate)"); font.bold: true }
                Label { text: estimate.resolution > 0 ? fmt(estimate.resolution, 2) + " " + AnalyzerController.tdrUnit : "--" }
            }

            Button {
                Layout.fillWidth: true
                text: AnalyzerController.tdrScanning ? qsTr("Stop") : qsTr("TDR Scan")
                highlighted: true
                enabled: AnalyzerController.connected && (AnalyzerController.tdrScanning || !AnalyzerController.measuring)
                onClicked: AnalyzerController.tdrScanning ? AnalyzerController.stop() : AnalyzerController.startTdr()
            }

            ProgressBar {
                Layout.fillWidth: true
                visible: AnalyzerController.tdrScanning
                value: AnalyzerController.tdrProgress
            }

            MenuSeparator { Layout.fillWidth: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 0
                Button {
                    text: qsTr("Impulse")
                    highlighted: page.trace === 0
                    Layout.fillWidth: true
                    onClicked: { page.trace = 0; page.selIndex = -1 }
                }
                Button {
                    text: qsTr("Step")
                    highlighted: page.trace === 1
                    Layout.fillWidth: true
                    onClicked: { page.trace = 1; page.selIndex = -1 }
                }
                Button {
                    text: qsTr("Impedance")
                    highlighted: page.trace === 2
                    Layout.fillWidth: true
                    onClicked: { page.trace = 2; page.selIndex = -1 }
                }
            }

            TdrChart {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                values: page.traceValues
                xStep: AnalyzerController.tdrXStep
                unit: AnalyzerController.tdrUnit
                selectedIndex: page.selIndex
                onPointSelected: (index) => page.selIndex = index
            }

            Label {
                text: page.selIndex >= 0 && page.selIndex < page.traceValues.length
                      ? qsTr("%1 %2:  %3").arg(fmt(page.selIndex * AnalyzerController.tdrXStep, 2))
                            .arg(AnalyzerController.tdrUnit).arg(fmt(page.traceValues[page.selIndex], 3))
                      : (AnalyzerController.tdrHasData ? qsTr("Touch the chart to read a point.")
                                                        : qsTr("Run a TDR scan to see the trace."))
                opacity: 0.8
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Label { text: qsTr("Window") }
                ComboBox {
                    Layout.fillWidth: true
                    model: [qsTr("Rectangular"), qsTr("Hamming"), qsTr("Hann"), qsTr("Blackman"), qsTr("Kaiser")]
                    currentIndex: AnalyzerController.tdrWindow
                    onActivated: (index) => AnalyzerController.tdrWindow = index
                }
                Label { text: qsTr("Beta"); visible: AnalyzerController.tdrWindow === 4 }
                TextField {
                    id: betaField
                    visible: AnalyzerController.tdrWindow === 4
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    Layout.preferredWidth: 70
                    onEditingFinished: AnalyzerController.tdrKaiserBeta = Number(text)
                }
            }

            Label { text: qsTr("Result"); font.bold: true }

            GridLayout {
                columns: 2
                columnSpacing: 24
                rowSpacing: 6
                Layout.fillWidth: true

                Label { text: qsTr("Distance to strongest reflection"); font.bold: true; wrapMode: Text.WordWrap; Layout.maximumWidth: column.width * 0.55 }
                Label {
                    text: !peak.found ? "--"
                          : !peak.aboveNoise ? qsTr("n/a (below noise floor)")
                          : fmt(peak.distance, 2) + " " + AnalyzerController.tdrUnit
                }
                Label { text: qsTr("Reflection"); font.bold: true }
                Label {
                    text: !peak.found ? "--"
                          : !peak.aboveNoise ? qsTr("None detected")
                          : (peak.amplitude > 0 ? qsTr("Open (≈ %1 Ω)") : qsTr("Short (≈ %1 Ω)")).arg(fmt(peak.impedance, 0))
                }
            }

            Label {
                visible: page.note !== ""
                text: page.note
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                color: "#c06000"
            }

            Label { text: qsTr("Known cable length"); font.bold: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                TextField {
                    id: knownField
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    Layout.fillWidth: true
                    placeholderText: AnalyzerController.tdrUnit
                }
                Label { text: qsTr("VF: %1").arg(page.calcVf > 0 ? fmt(page.calcVf, 3) : "--") }
                Button {
                    text: qsTr("Use")
                    enabled: page.calcVf > 0 && page.calcVf <= 1
                    onClicked: AnalyzerController.tdrVelocityFactor = page.calcVf
                }
            }
        }
    }

    CablePickerDialog { id: cablePicker }
}
