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
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
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
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
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

                // Single-finger marker select/pan, via a PointHandler rather
                // than the old mousePressEvent()-style overrides. A
                // PinchHandler was tried for two-finger zoom here, taking
                // the point over from this handler via Qt Quick's grab
                // negotiation -- it works for a trackpad's separate gesture
                // channel, but on a real touchscreen the single-finger grab
                // still wins in practice and a pinch never completes. So
                // touch pan/zoom goes through the ChartOverview strip below
                // instead, not PinchHandler.
                PointHandler {
                    target: null
                    acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Mouse | PointerDevice.Stylus
                    onActiveChanged: {
                        if (active) chart.beginSelect(point.position.x, point.position.y)
                        else chart.endSelect()
                    }
                    onPointChanged: if (active) chart.updateSelect(point.position.x)
                }

                // Mouse wheel and trackpad/Magic-Mouse glide (Qt can't tell
                // those two apart -- see the mouse/trackpad gesture testing
                // notes) zoom at the cursor. Harmless to leave enabled for
                // touch since a wheel event never arrives from a touchscreen.
                WheelHandler {
                    target: null
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: (event) => {
                        const delta = event.angleDelta.y !== 0 ? event.angleDelta.y : event.pixelDelta.y * 8
                        chart.wheelZoom(Math.pow(1.0015, delta), event.x)
                    }
                }
            }

            // Pan/zoom for touch: an absolute-position strip rather than a
            // pinch gesture (see the PointHandler comment above) -- also
            // better suited than pinch to a scan whose range can span
            // orders of magnitude, since it jumps anywhere in one drag
            // instead of many incremental pinch/re-grab cycles.
            ChartOverview {
                id: overview
                Layout.fillWidth: true
                onRangeChanged: (position, size) => chart.setViewWindow(position, size)
            }
            Binding { target: overview; property: "position"; value: chart.viewPosition; when: !overview.dragging }
            Binding { target: overview; property: "size"; value: chart.viewSize; when: !overview.dragging }

            // wrapMode + fillWidth matter here, not just cosmetics: a Label
            // left to its unwrapped implicit width becomes the effective
            // minimum width Qt Quick Layouts enforces for the whole shared
            // column, which was silently stretching the Scan button (and
            // chart) past the screen -- see the same fix on the other two
            // Labels in this column above.
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Touch the chart to read a point; drag the bar below to pan and zoom.")
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
