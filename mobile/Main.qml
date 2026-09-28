import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 400
    height: 800
    visible: true
    title: "AntScopeZ"

    readonly property var pages: [
        { title: qsTr("Connection"), source: "ConnectionPage.qml" },
        { title: qsTr("Scan"),       source: "ScanPage.qml" },
        { title: qsTr("Smith"),      source: "SmithPage.qml" },
        { title: qsTr("Settings"),   source: "SettingsPage.qml" }
    ]
    property int currentPage: 0

    function showPage(i) {
        currentPage = i
        stack.replace(null, pages[i].source)
        drawer.close()
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton {
                text: "☰"
                font.pixelSize: 22
                onClicked: drawer.open()
            }
            Label {
                text: window.pages[window.currentPage].title
                font.pixelSize: 20
                elide: Label.ElideRight
                Layout.fillWidth: true
            }
            Label {
                text: AnalyzerController.connected ? AnalyzerController.deviceName : qsTr("Not connected")
                opacity: 0.8
                rightPadding: 12
            }
        }
    }

    Drawer {
        id: drawer
        width: Math.min(window.width * 0.7, 300)
        height: window.height

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Label {
                text: "AntScopeZ"
                font.pixelSize: 22
                font.bold: true
                padding: 16
            }
            Repeater {
                model: window.pages
                ItemDelegate {
                    text: modelData.title
                    highlighted: index === window.currentPage
                    Layout.fillWidth: true
                    onClicked: window.showPage(index)
                }
            }
            Item { Layout.fillHeight: true }
            Label {
                text: "v" + Qt.application.version
                opacity: 0.6
                padding: 16
            }
        }
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: "ConnectionPage.qml"
    }

    footer: Label {
        text: AnalyzerController.status
        elide: Label.ElideRight
        padding: 8
        opacity: 0.8
    }
}
