import QtQuick 2.2
import QtQuick.Window 2.0
import QtQuick.Controls 1.1
import QtQuick.Controls.Styles 1.1

ApplicationWindow {
    id: backlight
    flags: Qt.FramelessWindowHint
    visible: true
    title: qsTr("backlight")
    width: 500
    height: 50
    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2
    color: "transparent"

    property real slideValue
    signal onSlide(real value)

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
            width: backlight.width - 16
            height: backlight.height
            value: backlight.slideValue
            focus: true
            onValueChanged: backlight.onSlide(value)
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
}
