pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Hyprland
import Quickshell.Io
import Quickshell.Wayland
import qs.Commons as Commons
import qs.Ui as Ui

Item {
  id: root

  property var shell: null
  property var values: null
  property bool saved: false
  property bool busy: false
  property bool writable: false
  property string operation: ""
  property string error: ""
  property bool nativeMissing: false
  property string loginStatus: ""
  property string loader: ""
  signal confirmed()

  readonly property string pluginId: "villevaisanen.workspace-ribbon"
  readonly property string helper: decodeURIComponent(String(Qt.resolvedUrl("../bin/ribbon-settings")).replace(/^(?:file|qs):\/\/(?=\/)/, ""))
  readonly property var typography: Commons.Style.font
  readonly property var spacing: Commons.Style.spacing
  readonly property color panelBorder: Commons.Color.popups["border"]
  readonly property real padding: spacing.popupPadding

  Commons.Translations { id: strings; directory: Qt.resolvedUrl("i18n") }

  function open(payloadJson) {
    if (!window.visible) {
      const monitor = Hyprland.focusedMonitor
      window.screen = Quickshell.screens.find(screen => monitor && screen.name === monitor.name)
                   || Quickshell.screens[0] || null
      window.visible = true
      Qt.callLater(() => headerSwitch.enabled ? headerSwitch.forceActiveFocus() : closeButton.forceActiveFocus())
    }
    run("get", [])
  }

  function close() {
    window.visible = false
  }

  function requestClose() {
    if (root.shell) root.shell.hide(root.pluginId)
    else root.close()
  }

  function current(key) {
    return values ? values[key] : 0
  }

  function run(action, args) {
    if (busy) return
    operation = action
    busy = true
    error = ""
    process.command = [helper, action, ...args]
    process.running = true
    watchdog.restart()
  }

  function change(key, value) {
    if (!writable || busy || current(key) === value) {
      confirmed()
      return
    }
    run("set", [key, String(value)])
  }

  function complete(output) {
    if (!busy) return
    watchdog.stop()
    busy = false
    try {
      const result = JSON.parse(output)
      if (result.ok !== true) {
        if (operation === "get" || result.code === "not_loaded")
          nativeMissing = result.code === "not_loaded"
        error = String(result.message || strings.t("helperError")).slice(0, 450)
        if (operation === "get" || result.code === "unavailable"
            || result.code === "not_loaded" || result.code === "incompatible")
          writable = false
      } else {
        values = result.values
        saved = result.saved
        writable = values !== null
        nativeMissing = values === null
        loginStatus = result.login || ""
        loader = String(result.loader || "").slice(0, 250)
      }
    } catch (err) {
      error = strings.t("helperError")
      writable = false
    }
    confirmed()
  }

  Process {
    id: process
    stdout: StdioCollector {
      onStreamFinished: root.complete(text)
    }
  }

  Timer {
    id: watchdog
    interval: root.operation === "build" ? 1250000 : 16000
    onTriggered: {
      root.complete("")
      process.running = false
    }
  }

  component PlainText: Text {
    textFormat: Text.PlainText
    color: Commons.Color.foreground
    font.family: root.typography.family
    font.pixelSize: root.typography.body
  }

  component DurationField: Ui.NumberField {
    id: durationField
    required property string option
    from: 1
    to: 10000
    stepSize: 1
    value: root.values ? root.current(option) : from
    enabled: root.writable
    onModified: newValue => root.change(option, newValue)
    Connections {
      target: root
      function onConfirmed() { durationField.field.value = durationField.value }
    }
  }

  PanelWindow {
    id: window
    visible: false
    anchors { top: true; right: true }
    margins { top: Commons.Style.gapsOut; right: Commons.Style.gapsOut }
    implicitWidth: Commons.Style.space(415)
    implicitHeight: body.implicitHeight + root.padding * 2
    color: "transparent"
    exclusionMode: ExclusionMode.Normal
    exclusiveZone: 0
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "workspace-ribbon-settings"
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.OnDemand

    Ui.BorderSurface {
      anchors.fill: parent
      color: Commons.Color.background
      radius: Commons.Style.cornerRadius
      borderSpec: Commons.Border.localOrSurfaceSpec("popups", "border",
                    root.panelBorder, root.panelBorder, 1)
    }

    FocusScope {
      anchors.fill: parent
      focus: true
      LayoutMirroring.enabled: Commons.I18n.rightToLeft
      LayoutMirroring.childrenInherit: true
      Keys.onEscapePressed: root.requestClose()

      ColumnLayout {
        id: body
        anchors { top: parent.top; left: parent.left; right: parent.right; margins: root.padding }
        spacing: root.spacing.lg

        RowLayout {
          Layout.fillWidth: true
          PlainText {
            Layout.fillWidth: true
            text: strings.t("title")
            font.pixelSize: root.typography.title
            font.bold: true
          }
          Ui.ToggleSwitch {
            id: headerSwitch
            enabled: root.writable
            busy: root.busy
            checked: root.current("enabled") === 1
            hasCursor: activeFocus
            activeFocusOnTab: true
            Accessible.name: strings.t("title")
            Accessible.role: Accessible.CheckBox
            Accessible.checked: checked
            onToggled: root.change("enabled", checked ? 0 : 1)
            Keys.onSpacePressed: root.change("enabled", checked ? 0 : 1)
            Keys.onReturnPressed: root.change("enabled", checked ? 0 : 1)
          }
          Ui.Button {
            id: closeButton
            text: "×"
            focusable: true
            Accessible.name: strings.t("close")
            onClicked: root.requestClose()
          }
        }

        ColumnLayout {
          visible: root.values !== null
          Layout.fillWidth: true
          spacing: root.spacing.md

          RowLayout {
            Layout.fillWidth: true
            PlainText { Layout.fillWidth: true; text: strings.t("zoom") }
            PlainText {
              text: strings.t("zoomScale").arg(Commons.I18n.formatNumber(zoom.dragging ? zoom.liveValue : zoom.value))
                                           .arg(Commons.I18n.formatNumber(100 - (zoom.dragging ? zoom.liveValue : zoom.value)))
            }
          }
          Ui.PanelSlider {
            id: zoom
            Layout.fillWidth: true
            minimum: 0
            maximum: 25
            step: 1
            integer: true
            trackColor: Commons.Style.selectedFillFor(Commons.Color.foreground, Commons.Color.accent)
            value: root.current("zoom_out_percent")
            enabled: root.writable
            activeFocusOnTab: true
            Accessible.name: strings.t("zoom")
            Accessible.role: Accessible.Slider
            onReleased: newValue => root.change("zoom_out_percent", newValue)
            Connections {
              target: root
              function onConfirmed() { if (!zoom.dragging) zoom.liveValue = zoom.value }
            }
            Keys.onLeftPressed: root.change("zoom_out_percent", Math.max(minimum, Math.min(maximum, value + (mirrored ? 1 : -1))))
            Keys.onRightPressed: root.change("zoom_out_percent", Math.max(minimum, Math.min(maximum, value + (mirrored ? -1 : 1))))
            Ui.BorderSurface {
              anchors.fill: parent
              visible: zoom.activeFocus
              color: "transparent"
              radius: Commons.Style.cornerRadius
              borderSpec: Commons.Border.controlSpec("focus", Commons.Color.foreground, Commons.Color.accent)
            }
          }

          RowLayout {
            Layout.fillWidth: true
            PlainText { Layout.fillWidth: true; text: strings.t("duration") }
            DurationField { option: "duration_ms"; Accessible.name: strings.t("duration") }
          }

          PlainText { text: strings.t("motion") }
          RowLayout {
            Layout.fillWidth: true
            Repeater {
              model: ["linear", "easeOut", "spring"]
              Ui.Button {
                required property int index
                required property string modelData
                Layout.fillWidth: true
                text: strings.t(modelData)
                bordered: true
                focusable: true
                enabled: root.writable
                selected: root.current("easing") === index
                onClicked: root.change("easing", index)
              }
            }
          }

          Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Commons.Style.normalFillFor(Commons.Color.foreground, Commons.Color.accent)
          }
          Ui.Toggle {
            Layout.fillWidth: true
            label: strings.t("scaleDistance")
            enabled: root.writable
            checked: root.current("scale_distance") === 1
            onClicked: root.change("scale_distance", checked ? 0 : 1)
          }
          RowLayout {
            Layout.fillWidth: true
            PlainText { Layout.fillWidth: true; text: strings.t("maximum") }
            DurationField { option: "max_duration_ms"; Accessible.name: strings.t("maximum") }
          }
          Ui.Toggle {
            Layout.fillWidth: true
            visible: root.loginStatus === "available" || root.loginStatus === "managed"
            enabled: root.writable && !root.busy
            label: strings.t("loadAtLogin")
            checked: root.loginStatus === "managed"
            onClicked: root.run("login", [checked ? "off" : "on"])
          }
          PlainText {
            Layout.fillWidth: true
            visible: root.loginStatus === "external"
            text: strings.t("externalLoader").arg(root.loader)
            font.pixelSize: root.typography.bodySmall
            wrapMode: Text.Wrap
          }
        }

        Ui.Button {
          Layout.fillWidth: true
          visible: root.nativeMissing
          enabled: !root.busy
          text: strings.t("buildEnable")
          bordered: true
          focusable: true
          onClicked: root.run("build", [])
        }

        PlainText {
          Layout.fillWidth: true
          visible: root.error !== ""
          text: root.error
          wrapMode: Text.WordWrap
          color: Commons.Color.urgent
        }

        RowLayout {
          Layout.fillWidth: true
          PlainText {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: root.typography.bodySmall
            text: root.busy ? strings.t(root.operation === "build" ? "building" : "working") :
                  root.error && root.values ? strings.t("lastConfirmed") :
                  root.values ? strings.t(root.saved ? "saved" : "live") : ""
          }
          Ui.Button {
            visible: root.error !== ""
            text: strings.t("retry")
            focusable: true
            bordered: true
            onClicked: root.run("get", [])
          }
          Ui.Button {
            visible: root.values !== null
            enabled: root.writable && !root.busy
            text: strings.t("save")
            focusable: true
            bordered: true
            onClicked: root.run("save", ["--expect", JSON.stringify(root.values)])
          }
        }
      }
    }
  }
}
