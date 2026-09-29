import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page
    property bool spanMode: false

    function refreshFields() {
        startField.text = AnalyzerController.fromKHz
        stopField.text = AnalyzerController.toKHz
        centerField.text = (AnalyzerController.fromKHz + AnalyzerController.toKHz) / 2
        spanField.text = AnalyzerController.toKHz - AnalyzerController.fromKHz
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

    Component.onCompleted: {
        refreshFields()
        pointsField.text = AnalyzerController.sweepPoints
        z0Field.text = AnalyzerController.z0
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        Label { text: qsTr("Band"); font.bold: true }

        GridLayout {
            columns: 2
            columnSpacing: 10
            rowSpacing: 6
            Layout.fillWidth: true

            Label { text: qsTr("ITU Region") }
            Label { text: qsTr("Band") }
            ComboBox {
                id: regionCombo
                Layout.fillWidth: true
                model: BandPresets.regions
                currentIndex: model.indexOf(BandPresets.region)
                onActivated: BandPresets.region = model[currentIndex]
            }
            ComboBox {
                id: bandCombo
                Layout.fillWidth: true
                model: BandPresets.bandLabels
                onActivated: {
                    const r = BandPresets.widenedRange(currentIndex)
                    if (r.fromKHz !== undefined) {
                        AnalyzerController.fromKHz = r.fromKHz
                        AnalyzerController.toKHz = r.toKHz
                        page.refreshFields()
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label { text: qsTr("Widen range, %") }
            SpinBox {
                id: widenSpin
                from: 0
                to: 100
                value: BandPresets.widenPercent
                onValueModified: BandPresets.widenPercent = value
                Layout.fillWidth: true
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

        Item { Layout.fillHeight: true }
    }
}
