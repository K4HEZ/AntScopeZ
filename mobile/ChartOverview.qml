import QtQuick

// Thin pan/zoom strip for a chart whose data can span a huge dynamic range
// (a sweep can run from 100 kHz to several GHz). It shows the full index
// range as a track, with a window rectangle marking the chart's current
// visible span: dragging the window body pans (an instant, absolute jump,
// not the pixel-rate crawl of edge-autoscroll), dragging either end dot
// resizes the window (zooms), and tapping the track outside the window
// jumps it there. It lives in its own strip below the plot, so its touch
// handling never competes with the plot's own marker-select drag (the
// reason mobile doesn't use PinchHandler for zoom -- see swrchart.h's
// beginSelect() comment).
Item {
    id: root
    implicitHeight: 32

    // 0..1 fractions of the full range; bind these one-way from the
    // chart's viewPosition/viewSize (via a Binding gated on !dragging, so
    // this strip's own interaction can drive them locally while active).
    property real position: 0
    property real size: 1
    property bool dragging: false

    // Emitted whenever the window moves or resizes; bind to the chart's
    // setViewWindow(position, size). Not named windowChanged: Item already
    // has a window property, so that name silently collides with its
    // auto-generated change signal (Qt logs "invalid override" and the
    // handler below would fire for the real window too).
    signal rangeChanged(real position, real size)

    readonly property real dotSize: 18     // visible end-handle diameter
    readonly property real hitSize: 36     // touch target around each dot --
                                            // bigger than the dot itself so
                                            // it's easy to grab without
                                            // stealing much of the body
    readonly property real minWinPx: 30    // window never renders narrower
                                            // than this, however small the
                                            // underlying fraction is
    // Smallest "size" fraction that still renders at minWinPx; depends on
    // the strip's current pixel width, so this is live, not fixed.
    readonly property real minSize: width > 0 ? Math.min(1, minWinPx / width) : 0.05

    function clampPos(p, s) { return Math.max(0, Math.min(1 - s, p)) }
    function clampSize(s) { return Math.max(root.minSize, Math.min(1, s)) }

    // Rendered window geometry, pixel-clamped defensively so it can never
    // extend past the strip's edges regardless of how position/size came
    // to be set -- this is what was overflowing off the right edge in
    // portrait before (a narrow screen makes minWinPx a bigger fraction of
    // the strip, so the mismatch between the fraction-space clamp and the
    // pixel-space render showed up there first).
    readonly property real rawWinW: size * width
    readonly property real winW: Math.max(minWinPx, rawWinW)
    readonly property real winX: Math.max(0, Math.min(position * width, width - winW))

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        // Fixed, theme-independent colors -- SystemPalette's greys came out
        // nearly invisible against the app's Material Light/Dark themes.
        color: Qt.rgba(0.5, 0.5, 0.5, 0.28)
        border.color: Qt.rgba(0.5, 0.5, 0.5, 0.6)
        border.width: 1
    }

    Rectangle {
        x: root.winX
        width: root.winW
        height: track.height
        radius: height / 2
        color: "#2196F3"
        opacity: 0.45
        border.color: "#2196F3"
        border.width: 2
    }

    // Tap outside the window: jump it here, centred on the tap. Sits below
    // the body/handles (declared first), so a tap inside the window goes to
    // them instead.
    MouseArea {
        anchors.fill: track
        onClicked: (mouse) => {
            const frac = mouse.x / width
            if (frac < root.position || frac > root.position + root.size) {
                const newPos = root.clampPos(frac - root.size / 2, root.size)
                root.position = newPos
                root.rangeChanged(newPos, root.size)
            }
        }
    }

    // Drag the window body: pan. Spans the full rendered window -- the end
    // dots sit on top of it in their own small hit area rather than a fixed
    // dead zone carved out of the body, which was making the body too easy
    // to miss ("too easy to hit the end vs the middle").
    //
    // All drag math below goes through mapToItem(root, ...) rather than
    // the handle's own (moving) local mouse.x: since winX/winW -- and so
    // this handle's position -- depend on root.position/root.size, which
    // the same drag is updating, a naive local-coordinate delta compounds
    // against its own motion each frame and visibly jitters (and feeds that
    // jitter straight into the chart via rangeChanged). mapToItem always
    // resolves against root's stationary frame, which cancels that out.
    MouseArea {
        id: body
        x: root.winX
        width: root.winW
        height: track.height
        property real startRootX: 0
        property real startPos: 0
        onPressed: (mouse) => {
            root.dragging = true
            startRootX = mapToItem(root, mouse.x, mouse.y).x
            startPos = root.position
        }
        onPositionChanged: (mouse) => {
            const curRootX = mapToItem(root, mouse.x, mouse.y).x
            const newPos = root.clampPos(startPos + (curRootX - startRootX) / root.width, root.size)
            root.position = newPos
            root.rangeChanged(newPos, root.size)
        }
        onReleased: root.dragging = false
    }

    // End handles: a small dot centred exactly on each edge of the window,
    // with a generous invisible hit area around it. The hit area's x is
    // clamped to stay within the strip's own [0, width] bounds -- at
    // position 0 the naive centred placement pokes hitSize/2 (18px) to the
    // left of the strip itself, which is enough to land past this page's
    // own margin and into the screen's edge-swipe zone for Main.qml's nav
    // Drawer, firing it by mistake instead of starting a drag here.
    Repeater {
        model: 2
        Item {
            readonly property bool isLeft: index === 0
            x: Math.max(0, Math.min(root.width - root.hitSize,
                                     (isLeft ? root.winX : root.winX + root.winW) - root.hitSize / 2))
            y: (track.height - root.hitSize) / 2
            width: root.hitSize
            height: root.hitSize

            Rectangle {
                anchors.centerIn: parent
                width: root.dotSize
                height: root.dotSize
                radius: width / 2
                color: "#2196F3"
                border.color: "white"
                border.width: 2
            }

            MouseArea {
                anchors.fill: parent
                property real startRootX: 0
                property real startPos: 0
                property real startSize: 0
                onPressed: (mouse) => {
                    root.dragging = true
                    startRootX = mapToItem(root, mouse.x, mouse.y).x
                    startPos = root.position
                    startSize = root.size
                }
                onPositionChanged: (mouse) => {
                    const curRootX = mapToItem(root, mouse.x, mouse.y).x
                    const frac = Math.max(0, Math.min(1, curRootX / root.width))
                    if (isLeft) {
                        const rightEdge = startPos + startSize
                        const newPos = Math.max(0, Math.min(frac, rightEdge - root.minSize))
                        root.position = newPos
                        root.size = root.clampSize(rightEdge - newPos)
                    } else {
                        root.size = Math.min(root.clampSize(frac - startPos), 1 - startPos)
                    }
                    root.rangeChanged(root.position, root.size)
                }
                onReleased: root.dragging = false
            }
        }
    }
}
