import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width - 32
        spacing: 12

        Image {
            source: "qrc:/AntScopeZ.png"
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: "AntScopeZ"
            font.pixelSize: 24
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: qsTr("Version %1").arg(Qt.application.version)
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: qsTr("Build %1").arg(buildTimestamp)
            opacity: 0.7
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            text: qsTr("Source Code:")
            font.bold: true
            Layout.topMargin: 12
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: "<a href=\"https://github.com/K4HEZ/AntScopeZ\">github.com/K4HEZ/AntScopeZ</a>"
            Layout.alignment: Qt.AlignHCenter
            onLinkActivated: (link) => Qt.openUrlExternally(link)
        }

        Label {
            text: qsTr("Web Page / Docs / Downloads:")
            font.bold: true
            Layout.topMargin: 8
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: "<a href=\"https://k4hez.github.io/AntScopeZ/\">k4hez.github.io/AntScopeZ/</a>"
            Layout.alignment: Qt.AlignHCenter
            onLinkActivated: (link) => Qt.openUrlExternally(link)
        }

        Label {
            text: "73, K4HEZ"
            Layout.topMargin: 16
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
