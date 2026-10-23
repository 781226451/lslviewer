import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1180
    height: 780
    minimumWidth: 880
    minimumHeight: 580
    visible: true
    title: "LSL Viewer · 流信息查看器"
    color: "#f3f5f7"
    font.family: Qt.platform.os === "windows" ? "Microsoft YaHei UI" : "sans-serif"
    font.pixelSize: 14
    palette.highlight: "#147d76"
    palette.highlightedText: "#ffffff"

    header: ToolBar {
        implicitHeight: 80
        padding: 18
        horizontalPadding: 18
        background: Rectangle { color: "#ffffff" }
        RowLayout {
            anchors.fill: parent
            spacing: 16
            Rectangle {
                width: 40; height: 40; radius: 10; color: "#147d76"
                Label { anchors.centerIn: parent; text: "LSL"; color: "white"; font.bold: true }
            }
            ColumnLayout {
                spacing: 2
                Label { text: "LSL Viewer"; font.pixelSize: 22; font.bold: true; color: "#1d2e3a" }
                Label { text: "Lab Streaming Layer / 流信息与元数据"; color: "#637582"; font.pixelSize: 12 }
            }
            Item { Layout.fillWidth: true }
            BusyIndicator { running: viewer.refreshing || viewer.loadingMetadata; implicitWidth: 30; implicitHeight: 30 }
            CheckBox { id: autoRefresh; text: "每 5 秒自动发现" }
            Button {
                objectName: "refreshButton"
                text: viewer.refreshing ? "正在发现…" : "刷新流列表"
                enabled: !viewer.refreshing
                onClicked: viewer.refresh()
            }
        }
    }

    SplitView {
        anchors.fill: parent
        anchors.margins: 18
        orientation: Qt.Horizontal
        handle: Rectangle { implicitWidth: 12; color: "transparent" }

        Frame {
            SplitView.preferredWidth: 330
            SplitView.minimumWidth: 270
            padding: 14
            background: Rectangle { color: "white"; radius: 12; border.color: "#e0e6eb" }
            ColumnLayout {
                anchors.fill: parent
                spacing: 12
                RowLayout {
                    Label { text: "可用流"; font.bold: true; font.pixelSize: 17; Layout.fillWidth: true }
                    Label { text: streamList.count + " 个"; color: "#637582" }
                }
                Label { text: "选择流以读取完整信息"; color: "#637582"; font.pixelSize: 12 }
                ListView {
                    id: streamList
                    objectName: "streamList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 8
                    model: viewer.streamModel
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        id: streamDelegate
                        required property string uid
                        required property string name
                        required property string streamType
                        required property int channelCount
                        required property real nominalSrate
                        required property string sourceId
                        width: streamList.width
                        padding: 13
                        highlighted: uid === viewer.selectedUid
                        onClicked: viewer.selectStream(uid)
                        background: Rectangle {
                            radius: 8
                            color: streamDelegate.highlighted ? "#e9f5f2" : (streamDelegate.hovered ? "#f3f5f7" : "#ffffff")
                            border.color: streamDelegate.highlighted ? "#8bc2b9" : "#e4e9ed"
                        }
                        contentItem: ColumnLayout {
                            spacing: 7
                            Label {
                                text: streamDelegate.name || "未命名流"
                                textFormat: Text.PlainText
                                font.bold: true
                                color: "#1d2e3a"
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: (streamDelegate.streamType || "未知类型") + " · " + streamDelegate.channelCount + " ch · " + (streamDelegate.nominalSrate === 0 ? "不规则采样" : streamDelegate.nominalSrate + " Hz")
                                textFormat: Text.PlainText
                                color: "#637582"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: "Source: " + (streamDelegate.sourceId || "未提供")
                                textFormat: Text.PlainText
                                color: "#637582"
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        width: parent.width - 24
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        visible: streamList.count === 0
                        text: viewer.refreshing ? "正在搜索 LSL 流…" : "尚未发现流\n启动 LSL 发布端后，点击刷新。"
                        color: "#637582"
                    }
                }
            }
        }

        Frame {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 530
            padding: 20
            background: Rectangle { color: "white"; radius: 12; border.color: "#e0e6eb" }
            ColumnLayout {
                anchors.fill: parent
                spacing: 16
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label {
                            text: viewer.selectedUid ? viewer.details.name || "未命名流" : "流信息"
                            textFormat: Text.PlainText
                            font.bold: true
                            font.pixelSize: 23
                            color: "#1d2e3a"
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        Label { text: "属性 · 结构化 metadata · 原始 XML"; color: "#637582"; font.pixelSize: 12 }
                    }
                    Button {
                        text: "重新读取"
                        enabled: viewer.selectedUid !== "" && !viewer.loadingMetadata
                        onClicked: viewer.selectStream(viewer.selectedUid)
                    }
                }
                TabBar {
                    id: tabs
                    objectName: "detailTabs"
                    Layout.fillWidth: true
                    TabButton { text: "流属性" }
                    TabButton { text: "Metadata" }
                    TabButton { text: "完整 XML" }
                }
                StackLayout {
                    currentIndex: tabs.currentIndex
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ListView {
                        id: propertiesView
                        clip: true
                        spacing: 2
                        model: viewer.selectedUid ? [
                            { label: "Name / 名称", value: viewer.details.name || "—" },
                            { label: "Type / 类型", value: viewer.details.type || "—" },
                            { label: "Source ID", value: viewer.details.source_id || "未提供" },
                            { label: "UID", value: viewer.details.uid || "—" },
                            { label: "Host / 主机", value: viewer.details.hostname || "—" },
                            { label: "Channels / 通道数", value: viewer.details.channel_count ?? "—" },
                            { label: "Nominal rate / 采样率", value: viewer.details.nominal_srate === undefined ? "—" : (viewer.details.nominal_srate === 0 ? "不规则采样" : viewer.details.nominal_srate + " Hz") },
                            { label: "Channel format", value: viewer.details.channel_format || "—" },
                            { label: "LSL protocol version", value: viewer.details.version ?? "—" },
                            { label: "Created at / LSL 时钟", value: viewer.details.created_at ?? "—" }
                        ] : []
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            width: propertiesView.width
                            height: Math.max(46, propertyRow.implicitHeight + 16)
                            radius: 6
                            color: index % 2 === 0 ? "#f5f7f9" : "white"
                            RowLayout {
                                id: propertyRow
                                anchors.left: parent.left; anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: 12
                                spacing: 12
                                Label { text: modelData.label; color: "#637582"; Layout.preferredWidth: 175; wrapMode: Text.Wrap }
                                TextEdit {
                                    text: String(modelData.value)
                                    font.family: window.font.family
                                    font.pixelSize: 14
                                    textFormat: TextEdit.PlainText
                                    color: "#1d2e3a"
                                    readOnly: true
                                    selectByMouse: true
                                    wrapMode: TextEdit.WrapAnywhere
                                    Layout.fillWidth: true
                                }
                            }
                        }
                        Label { anchors.centerIn: parent; visible: !viewer.selectedUid; text: "从左侧选择一个流，查看其属性。"; color: "#637582" }
                    }
                    ListView {
                        id: metadataView
                        objectName: "metadataView"
                        clip: true
                        spacing: 2
                        model: viewer.metadataModel
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            required property string name
                            required property string value
                            required property int depth
                            required property int index
                            width: metadataView.width
                            height: Math.max(38, metadataRow.implicitHeight + 14)
                            color: index % 2 === 0 ? "#f5f7f9" : "white"
                            radius: 5
                            RowLayout {
                                id: metadataRow
                                anchors.left: parent.left; anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: 10
                                spacing: 12
                                TextEdit {
                                    text: "  ".repeat(depth) + name
                                    textFormat: TextEdit.PlainText
                                    color: "#147d76"
                                    font.family: "Consolas"
                                    font.pixelSize: 14
                                    readOnly: true
                                    selectByMouse: true
                                    wrapMode: TextEdit.WrapAnywhere
                                    Layout.preferredWidth: metadataView.width * 0.45
                                }
                                TextEdit {
                                    text: value
                                    font.family: window.font.family
                                    font.pixelSize: 14
                                    textFormat: TextEdit.PlainText
                                    color: "#1d2e3a"
                                    readOnly: true
                                    selectByMouse: true
                                    wrapMode: TextEdit.WrapAnywhere
                                    Layout.fillWidth: true
                                }
                            }
                        }
                        Label {
                            anchors.centerIn: parent
                            width: parent.width - 24
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            visible: metadataView.count === 0
                            text: viewer.loadingMetadata ? "正在读取 metadata…" : (viewer.xml ? "该流没有 desc metadata。" : "选择流并成功读取后，将在这里显示 metadata。")
                            color: "#637582"
                        }
                    }
                    ColumnLayout {
                        RowLayout {
                            Label { text: "完整 StreamInfo XML"; color: "#637582"; Layout.fillWidth: true }
                            Button { text: "复制 XML"; enabled: viewer.xml !== ""; onClicked: viewer.copyXml() }
                        }
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            TextArea {
                                objectName: "xmlText"
                                text: viewer.xml
                                textFormat: TextEdit.PlainText
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.NoWrap
                                font.family: "Consolas"
                                font.pixelSize: 13
                                color: "#1d2e3a"
                                placeholderText: "完整 XML 将在读取成功后显示。"
                                background: Rectangle { color: "#f5f7f9"; radius: 6 }
                            }
                        }
                    }
                }
            }
        }
    }
    footer: ToolBar {
        implicitHeight: Math.max(44, statusLabel.implicitHeight + 24)
        padding: 12
        horizontalPadding: 18
        background: Rectangle { color: "#eaf0f3" }
        Label {
            id: statusLabel
            width: parent.width
            text: viewer.status
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: viewer.status.includes("失败") ? "#a83333" : "#455d6e"
        }
    }
    Timer {
        interval: 5000
        repeat: true
        running: autoRefresh.checked
        onTriggered: viewer.refresh()
    }
    Component.onCompleted: viewer.refresh()
}
