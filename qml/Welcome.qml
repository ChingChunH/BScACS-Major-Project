import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

Window {
    id: welcomeWindow
    width: 400
    height: 300
    visible: true
    title: "Welcome"

    Material.theme: Material.Light
    Material.accent: Material.DeepPurple

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#f5f5f5" }
            GradientStop { position: 1.0; color: "#e0e0e0" }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 20
        Layout.preferredWidth: parent.width * 0.8

        Text {
            text: "Choose Your Operating System"
            font.pixelSize: 24
            color: "#424242"
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Qt.AlignHCenter
        }

        Button {
            text: "Windows"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: parent.width * 0.6
            font.pixelSize: 18
            Material.foreground: "white"
            Material.background: Material.DeepPurple
            // Optionally, you can customize the background using a Rectangle:
            // background: Rectangle {
            //     radius: 6
            //     color: Material.DeepPurple
            // }
            onClicked: {
                console.log("Opening Windows Dashboard")
                let windows = windowsDashboardPage.createObject()
                if (windows) {
                    windows.show()
                    welcomeWindow.close()
                } else {
                    console.log("Error: Failed to create Windows Dashboard")
                }
            }
        }

        // macOS Button with updated Material styling and width
        Button {
            text: "macOS"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: parent.width * 0.6
            font.pixelSize: 18
            Material.foreground: "white"
            Material.background: Material.DeepPurple
            onClicked: {
                console.log("Opening macOS Dashboard")
                let mac = macosDashboardPage.createObject()
                if (mac) {
                    mac.show()
                    welcomeWindow.close()
                } else {
                    console.log("Error: Failed to create macOS Dashboard")
                }
            }
        }
    }
}
