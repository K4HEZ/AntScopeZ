import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    // Dark green at 1:1, yellow 3:1, orange 5:1, red toward 10:1.
    readonly property var swrStops: [
        { swr: 1,  r: 0.04, g: 0.48, b: 0.12 },
        { swr: 3,  r: 0.90, g: 0.78, b: 0.00 },
        { swr: 5,  r: 1.00, g: 0.55, b: 0.00 },
        { swr: 10, r: 0.82, g: 0.00, b: 0.00 }
    ]
    function swrColor(swr) {
        if (swr <= swrStops[0].swr)
            return Qt.rgba(swrStops[0].r, swrStops[0].g, swrStops[0].b, 1)
        for (var i = 1; i < swrStops.length; i++) {
            if (swr <= swrStops[i].swr) {
                const a = swrStops[i - 1], b = swrStops[i]
                const t = (swr - a.swr) / (b.swr - a.swr)
                return Qt.rgba(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1)
            }
        }
        const last = swrStops[swrStops.length - 1]
        return Qt.rgba(last.r, last.g, last.b, 1)
    }

    Component.onCompleted: fqField.text = ((AnalyzerController.fromKHz + AnalyzerController.toKHz) / 2).toFixed(0)

    // The device does one thing at a time; don't leave a live loop blocking Scan.
    Component.onDestruction: {
        if (AnalyzerController.liveMode)
            AnalyzerController.stop()
    }

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

        ParametersGrid {
            point: AnalyzerController.livePoint
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("SWR")
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        // Fills the rest of the page; Text.Fit shrinks the number to whatever
        // size the screen allows.
        Label {
            readonly property bool valid: AnalyzerController.livePoint.swr !== undefined
            text: valid ? Number(AnalyzerController.livePoint.swr).toFixed(2) : "--"
            color: valid ? swrColor(AnalyzerController.livePoint.swr) : palette.text
            font.pixelSize: 1000
            font.bold: true
            fontSizeMode: Text.Fit
            minimumPixelSize: 24
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
