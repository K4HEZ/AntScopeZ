import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Parameter readout for one point (a map from AnalyzerController).
GridLayout {
    id: grid
    property var point: ({})
    readonly property bool valid: point !== null && point !== undefined && point.fq !== undefined

    function fmt(v) { return v === undefined ? "--" : Number(v).toFixed(2) }
    function reactive(r, x) {
        return fmt(r) + (x < 0 ? " − j" : " + j") + fmt(Math.abs(x))
    }

    columns: 2
    columnSpacing: 24
    rowSpacing: 6

    Label { text: qsTr("Frequency, MHz"); font.bold: true }
    Label { text: valid ? Number(point.fq).toFixed(3) : "--" }

    Label { text: qsTr("SWR"); font.bold: true }
    Label { text: valid ? fmt(point.swr) : "--" }

    Label { text: qsTr("Return Loss, dB"); font.bold: true }
    Label { text: valid ? fmt(point.rl) : "--" }

    Label { text: qsTr("R + jX, Ω (series)"); font.bold: true }
    Label { text: valid ? reactive(point.r, point.x) : "--" }

    Label { text: qsTr("|Z|, Ω"); font.bold: true }
    Label { text: valid ? fmt(point.z) : "--" }

    Label { text: qsTr("R + jX, Ω (parallel)"); font.bold: true }
    Label { text: valid ? reactive(point.rpar, point.xpar) : "--" }

    Label { text: qsTr("|Zp|, Ω"); font.bold: true }
    Label { text: valid ? fmt(point.zpar) : "--" }

    Label { text: qsTr("Phase, °"); font.bold: true }
    Label { text: valid ? fmt(point.rhoPhase) : "--" }
}
