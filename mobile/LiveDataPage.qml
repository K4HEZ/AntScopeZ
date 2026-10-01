import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    // A live reading is a narrow 3-point span around the target frequency
    // (the Match rejects a zero-span request); show whichever of the three
    // lands closest to it rather than assuming an index.
    readonly property var point: {
        const pts = AnalyzerController.points
        const targetMHz = Number(fqField.text) / 1000
        var best = null, bestDelta = -1
        for (var i = 0; i < pts.length; i++) {
            const delta = Math.abs(pts[i].fq - targetMHz)
            if (best === null || delta < bestDelta) {
                best = pts[i]
                bestDelta = delta
            }
        }
        return best
    }

    function fmt(v) { return v === undefined ? "--" : Number(v).toFixed(2) }
    function fmtFq(mhz) { return Number(mhz).toFixed(3) }

    Component.onCompleted: fqField.text = ((AnalyzerController.fromKHz + AnalyzerController.toKHz) / 2).toFixed(0)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label { text: qsTr("Frequency, kHz") }
            TextField {
                id: fqField
                Layout.fillWidth: true
                enabled: !AnalyzerController.liveMode
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }
        }

        Button {
            Layout.fillWidth: true
            text: AnalyzerController.liveMode ? qsTr("Stop") : qsTr("Start")
            enabled: AnalyzerController.connected && (!AnalyzerController.measuring || AnalyzerController.liveMode)
            highlighted: true
            onClicked: AnalyzerController.liveMode
                       ? AnalyzerController.stop()
                       : AnalyzerController.startLive(Number(fqField.text))
        }

        Label {
            visible: point !== null
            text: point ? fmtFq(point.fq) + " MHz" : ""
            font.pixelSize: 18
            font.bold: true
        }

        GridLayout {
            columns: 2
            columnSpacing: 24
            rowSpacing: 6
            Layout.fillWidth: true

            Label { text: qsTr("SWR"); font.bold: true }
            Label { text: point ? fmt(point.swr) : "--" }

            Label { text: qsTr("Return Loss, dB"); font.bold: true }
            Label { text: point ? fmt(point.rl) : "--" }

            Label { text: qsTr("R + jX, Ω (series)"); font.bold: true }
            Label {
                text: point ? fmt(point.r) + (point.x < 0 ? " − j" : " + j") + fmt(Math.abs(point.x)) : "--"
            }

            Label { text: qsTr("|Z|, Ω"); font.bold: true }
            Label { text: point ? fmt(point.z) : "--" }

            Label { text: qsTr("R + jX, Ω (parallel)"); font.bold: true }
            Label {
                text: point ? fmt(point.rpar) + (point.xpar < 0 ? " − j" : " + j") + fmt(Math.abs(point.xpar)) : "--"
            }

            Label { text: qsTr("|Zp|, Ω"); font.bold: true }
            Label { text: point ? fmt(point.zpar) : "--" }

            Label { text: qsTr("Phase, °"); font.bold: true }
            Label { text: point ? fmt(point.rhoPhase) : "--" }
        }

        Item { Layout.fillHeight: true }
    }
}
