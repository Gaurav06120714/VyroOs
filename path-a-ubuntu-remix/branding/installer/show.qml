/* Vyro OS — Calamares installer slideshow
 * Shown while the installer copies files. Auto-advances every 10s.
 */
import QtQuick 2.15;
import calamares.slideshow 1.0;

Presentation {
    id: presentation

    Timer {
        interval: 10000
        running: presentation.activatedInCalamares
        repeat: true
        onTriggered: presentation.goToNextSlide()
    }

    Slide {
        Image {
            id: slide1bg
            anchors.fill: parent
            source: "slide-1.png"
            fillMode: Image.PreserveAspectCrop
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 40
            color: "#E8E9F1"
            text: "Welcome to Vyro OS — a desktop that respects your time."
            font.family: "Inter"
            font.pixelSize: 22
            font.weight: Font.DemiBold
        }
    }

    Slide {
        Image { anchors.fill: parent; source: "slide-2.png"; fillMode: Image.PreserveAspectCrop }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 40
            color: "#E8E9F1"
            text: "Glass surfaces, accent purple, real keyboard shortcuts."
            font.family: "Inter"; font.pixelSize: 22; font.weight: Font.DemiBold
        }
    }

    Slide {
        Image { anchors.fill: parent; source: "slide-3.png"; fillMode: Image.PreserveAspectCrop }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 40
            color: "#E8E9F1"
            text: "Ubuntu 24.04 LTS underneath — runs all the apps you already use."
            font.family: "Inter"; font.pixelSize: 22; font.weight: Font.DemiBold
        }
    }

    Slide {
        Image { anchors.fill: parent; source: "slide-4.png"; fillMode: Image.PreserveAspectCrop }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 40
            color: "#E8E9F1"
            text: "Made by one person. Free, MIT-licensed, here to stay."
            font.family: "Inter"; font.pixelSize: 22; font.weight: Font.DemiBold
        }
    }
}
