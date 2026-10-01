import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page
    property bool spanMode: false

    // String() avoids the "1e+07" / 6-digit rounding of a bare number-to-text.
    function fmtKHz(v) { return String(Math.round(v * 1000) / 1000) }

    function refreshFields() {
        startField.text = fmtKHz(AnalyzerController.fromKHz)
        stopField.text = fmtKHz(AnalyzerController.toKHz)
        centerField.text = fmtKHz((AnalyzerController.fromKHz + AnalyzerController.toKHz) / 2)
        spanField.text = fmtKHz(AnalyzerController.toKHz - AnalyzerController.fromKHz)
    }

    function applyStartStop() {
        AnalyzerController.fromKHz = Number(startField.text)
        AnalyzerController.toKHz = Number(stopField.text)
        refreshFields()
    }

    function applyCenterSpan() {
        const span = Number(spanField.text)
        const center = Number(centerField.text)
        AnalyzerController.fromKHz = center - span / 2
        AnalyzerController.toKHz = center + span / 2
        refreshFields()
    }

    Connections {
        target: AnalyzerController
        function onFromKHzChanged() { page.refreshFields() }
        function onToKHzChanged() { page.refreshFields() }
    }

    Component.onCompleted: {
        refreshFields()
        pointsField.text = AnalyzerController.sweepPoints
        z0Field.text = AnalyzerController.z0
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Button {
            Layout.fillWidth: true
            text: AnalyzerController.measuring ? qsTr("Stop") : qsTr("Scan")
            enabled: AnalyzerController.connected
            highlighted: true
            onClicked: AnalyzerController.measuring ? AnalyzerController.stop() : AnalyzerController.scan()
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: column.implicitHeight
            clip: true

            ColumnLayout {
                id: column
                width: parent.width
                spacing: 16

                Label { text: qsTr("Band"); font.bold: true }

                ComboBox {
                    id: regionCombo
                    Layout.fillWidth: true
                    implicitHeight: 56
                    font.pixelSize: 16
                    model: BandPresets.regions
                    currentIndex: model.indexOf(BandPresets.region)
                    onActivated: BandPresets.region = model[currentIndex]
                }
                ComboBox {
                    id: bandCombo
                    Layout.fillWidth: true
                    model: [qsTr("Select a band")].concat(BandPresets.bandLabels)
                    onActivated: (index) => {
                        if (index > 0) {
                            const r = BandPresets.widenedRange(index - 1)
                            if (r.fromKHz !== undefined) {
                                AnalyzerController.fromKHz = r.fromKHz
                                AnalyzerController.toKHz = r.toKHz
                                page.refreshFields()
                            }
                        }
                        currentIndex = 0
                    }
                }

                Label { text: qsTr("Frequency Range"); font.bold: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Button {
                        text: qsTr("Start / Stop")
                        highlighted: !page.spanMode
                        Layout.fillWidth: true
                        onClicked: page.spanMode = false
                    }
                    Button {
                        text: qsTr("Center / Span")
                        highlighted: page.spanMode
                        Layout.fillWidth: true
                        onClicked: page.spanMode = true
                    }
                }

                GridLayout {
                    columns: 2
                    visible: !page.spanMode
                    columnSpacing: 10
                    rowSpacing: 6
                    Layout.fillWidth: true

                    Label { text: qsTr("Start, kHz") }
                    Label { text: qsTr("Stop, kHz") }
                    TextField {
                        id: startField
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        Layout.fillWidth: true
                        onEditingFinished: page.applyStartStop()
                    }
                    TextField {
                        id: stopField
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        Layout.fillWidth: true
                        onEditingFinished: page.applyStartStop()
                    }
                }

                GridLayout {
                    columns: 2
                    visible: page.spanMode
                    columnSpacing: 10
                    rowSpacing: 6
                    Layout.fillWidth: true

                    Label { text: qsTr("Center, kHz") }
                    Label { text: qsTr("Span, kHz") }
                    TextField {
                        id: centerField
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        Layout.fillWidth: true
                        onEditingFinished: page.applyCenterSpan()
                    }
                    TextField {
                        id: spanField
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        Layout.fillWidth: true
                        onEditingFinished: page.applyCenterSpan()
                    }
                }

                Label { text: qsTr("Sweep"); font.bold: true }

                GridLayout {
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 6
                    Layout.fillWidth: true

                    Label { text: qsTr("Points") }
                    Label { text: qsTr("Z0, Ω") }
                    TextField {
                        id: pointsField
                        inputMethodHints: Qt.ImhDigitsOnly
                        Layout.fillWidth: true
                        onEditingFinished: AnalyzerController.sweepPoints = parseInt(text)
                    }
                    TextField {
                        id: z0Field
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        Layout.fillWidth: true
                        onEditingFinished: AnalyzerController.z0 = Number(text)
                    }
                }
            }
        }
    }
}
