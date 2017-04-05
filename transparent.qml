import QtQuick 2.2
import QtQuick.Window 2.0
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2

ApplicationWindow {
    id: window_transparent
    flags: Qt.FramelessWindowHint
    visible: true
    title: qsTr("transparent controls")
    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2
    color: "transparent"
    width: Screen.width
    height: Screen.height

    Rectangle {
        anchors.centerIn: parent
        width: parent.width
        height: 50
        color: "transparent"

        Rectangle {
            anchors.fill: parent
            radius: 25
            opacity: 0.3
            color: "gray"
        }

        Slider {
            anchors.centerIn: parent
            width: window_transparent.width - 16

            maximumValue: 255
            minimumValue: 0
            value: 128
            focus: true
            onValueChanged: pipeline.setBrightness(parseInt(value))
            Keys.onSpacePressed: Qt.quit()
            Keys.onEscapePressed: Qt.quit()

            style: SliderStyle {
                groove: Rectangle {
                    implicitHeight: 8
                    radius: 4
                    color: "gray"
                }
                handle: Rectangle {
                    anchors.centerIn: parent
                    color: control.pressed ? "white" : "lightgray"
                    border.color: "gray"
                    border.width: 2
                    width: 34
                    height: 34
                    radius: 17
                }
            }
        }
    }

    Button {
        id: button_record
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 10

        state: "off"

        style: ButtonStyle {
            label: Text {
                text: button_record.state == "off" ? "\u2B24" : "\u25A0"
                color: button_record.state == "off" ? "#ff0000" : "#000000"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                implicitWidth: 40
                implicitHeight: 40
                border.width: control.activeFocus ? 2 : 1
                border.color: "#888"
                radius: 20
                gradient: Gradient {
                    GradientStop { position: 0 ; color: button_record.state == "on" ? "#ccc" : "#eee" }
                    GradientStop { position: 1 ; color: button_record.state == "on" ? "#aaa" : "#ccc" }
                }
            }
        }

        states: [
            State { name: "off" },
            State { name: "on" }
        ]

        onClicked: {
            state == "on" ? pipeline.stop() : pipeline.record()
            state == "on" ? state = "off" : state = "on"
        }
    }
}
