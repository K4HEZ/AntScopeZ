import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: picker
    parent: Overlay.overlay
    modal: true
    x: 8
    y: 8
    width: parent.width - 16
    height: parent.height - 16
    padding: 12
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property bool filtering: search.text.trim() !== ""
    readonly property var recents: AnalyzerController.tdrRecentCables
    // Recent first, then the rest; just the matches while searching.
    readonly property var rows: {
        if (filtering)
            return AnalyzerController.tdrFilterCables(search.text).map(n => ({ name: n, group: "" }))
        const all = AnalyzerController.tdrFilterCables("")
        return recents.map(n => ({ name: n, group: qsTr("Recent") }))
            .concat(all.map(n => ({ name: n, group: qsTr("All cables") })))
    }

    onAboutToShow: { search.text = ""; list.positionViewAtBeginning() }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: search
                Layout.fillWidth: true
                placeholderText: qsTr("Search cables")
                inputMethodHints: Qt.ImhNoPredictiveText
            }
            Button {
                text: qsTr("Cancel")
                onClicked: picker.close()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: picker.rows
            section.property: "group"
            section.delegate: Label {
                width: list.width
                visible: text !== ""
                height: visible ? implicitHeight + 8 : 0
                text: section
                font.bold: true
                verticalAlignment: Text.AlignBottom
            }
            delegate: ItemDelegate {
                width: list.width
                text: modelData.name
                highlighted: modelData.name === AnalyzerController.tdrCableName
                onClicked: {
                    // Close first: selecting rebuilds rows and destroys this delegate.
                    const name = modelData.name
                    picker.close()
                    AnalyzerController.tdrSelectCable(name)
                }
            }
            ScrollBar.vertical: ScrollBar {}
            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("No matching cables")
                opacity: 0.6
            }
        }
    }
}
