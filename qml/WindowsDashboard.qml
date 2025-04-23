import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtCharts 2.1
import Monitor.Database 1.0
import QtQuick.Controls.Material 2.15

ApplicationWindow {
    id: window
    width: 1000
    height: 700
    visible: true
    title: qsTr("System Monitor")
    Material.theme: Material.theme
    Material.accent: Material.Blue
    font.family: "Roboto"

    // ----------------------------------------------------------------
    // Monitoring, Logging, & Chart Data Properties
    // ----------------------------------------------------------------
    property string monitoringStatus: "Waiting..."
    property var logMessages: []
    property var criticalChanges: []
    property var searchResults: []

    // Chart data properties
    property var chartCategories: []
    property var barSetModel: []

    // ----------------------------------------------------------------
    // Logging and Critical Changes Functions
    // ----------------------------------------------------------------
    function addLog(message) {
        logMessages.push(message)
        // Update the text area in the Logs tab
        logArea.text = logMessages.join("\n")
    }

    Timer {
        id: removalTimer
        interval: 10000
        repeat: false
        running: false
        onTriggered: {
            if (criticalChanges.length > 0) {
                criticalChanges = []
                criticalChanges.push({ "changeText": "No changes detected yet." })
                criticalChangesListView.model = criticalChanges
            }
        }
    }

    function addCriticalChange(message) {
        if (typeof message !== "string") {
            console.error("addCriticalChange expects a string message. Got:", message)
            return
        }
        // Remove placeholder entry if present.
        for (let i = 0; i < criticalChanges.length; i++) {
            if (criticalChanges[i].changeText === "No changes detected yet.") {
                criticalChanges.splice(i, 1)
                break
            }
        }
        criticalChanges.push({ "changeText": message, "cancelled": false })
        criticalChangesListView.model = criticalChanges
        removalTimer.start()
    }

    function cleanTimestamp(ts) {
        if (!ts) return "N/A";
        // Convert Date object to string if necessary.
        let tsStr = ts.toString();
        // Remove trailing timezone info starting with "GMT" (case-insensitive)
        return tsStr.replace(/gmt.*$/i, "").trim();
    }

    // Format the search results for display in a TextArea
    function formatSearchResults(results) {
        let formatted = "";
        results.forEach((result) => {
            if (!result) {
                formatted += "[null result]\n";
                return;
            }
            let cleanedDate = cleanTimestamp(result.timestamp);
            formatted += "Configuration: " + (result.config_name || "N/A") +
                         " | Approved: " + (result.acknowledged ? "true" : "false") +
                         " | Critical: " + (result.critical ? "true" : "false") +
                         " | Date: " + cleanedDate + "\n";
        });
        return formatted.trim();
    }

    // Function to update the search results immediately based on current filters.
    function updateSearchResults() {
        // Convert date fields if provided:
        function toFullDateTime(inputDate, isStart) {
            if (!inputDate || inputDate.trim() === "") {
                return "";
            }
            let dateRegex = /^\d{4}-\d{2}-\d{2}$/;
            if (dateRegex.test(inputDate.trim())) {
                return isStart
                    ? inputDate.trim() + " 00:00:00"
                    : inputDate.trim() + " 23:59:59";
            }
            return inputDate.trim();
        }
        let start = toFullDateTime(searchStartDateField.text, true);
        let end = toFullDateTime(searchEndDateField.text, false);

        // Build the approval filter:
        // (This example uses check boxes. When clicked, they immediately update the results.)
        let ackFilter = null;
        if (acknowledgedOnlyCheckBox.checked && !unacknowledgedOnlyCheckBox.checked) {
            ackFilter = true;
        } else if (!acknowledgedOnlyCheckBox.checked && unacknowledgedOnlyCheckBox.checked) {
            ackFilter = false;
        }
        // Build the critical filter:
        let criticalFilter = null;
        if (criticalOnlyCheckBox.checked && !nonCriticalOnlyCheckBox.checked) {
            criticalFilter = true;
        } else if (!criticalOnlyCheckBox.checked && nonCriticalOnlyCheckBox.checked) {
            criticalFilter = false;
        }

        console.log("Checkbox update: Approved =", acknowledgedOnlyCheckBox.checked,
                    ", Disapproved =", unacknowledgedOnlyCheckBox.checked,
                    ", Critical =", criticalOnlyCheckBox.checked,
                    ", Non-critical =", nonCriticalOnlyCheckBox.checked);
        console.log("Search params: start =", start, ", end =", end,
                    ", configName =", searchKeyNameField.text,
                    ", ackFilter =", ackFilter, ", criticalFilter =", criticalFilter);

        let results = Database.searchChangeHistoryRange(
            start,
            end,
            searchKeyNameField.text,
            ackFilter,
            criticalFilter
        );
        console.log("Updated search results:", results);
        window.searchResults = results ? results : [];
    }

    // Build stacked bar data for the Chart
    function buildStackedBarData(data) {
        var dates = []
        var configNames = []
        var dataMap = {}

        for (var i = 0; i < data.length; i++) {
            var row = data[i]
            var date = row.date
            var configName = row.config_name
            var count = row.count

            if (dates.indexOf(date) === -1) {
                dates.push(date)
            }
            if (configNames.indexOf(configName) === -1) {
                configNames.push(configName)
            }
            var key = configName + "_" + date
            dataMap[key] = count
        }
        dates.sort()
        console.log("Data for the past 7 days:", data)

        var barSets = []
        for (var c = 0; c < configNames.length; c++) {
            var config = configNames[c]
            var values = []
            for (var d = 0; d < dates.length; d++) {
                var dateKey = dates[d]
                var lookup = config + "_" + dateKey
                values.push(dataMap[lookup] !== undefined ? dataMap[lookup] : 0)
            }
            barSets.push({ "label": config, "values": values })
        }
        return { "dates": dates, "barSets": barSets }
    }

    // ----------------------------------------------------------------
    // Initialize the chart data when the component is completed.
    // ----------------------------------------------------------------
    Component.onCompleted: {
        let data = Database.getChangesCountByDateAndConfig()
        let chartData = buildStackedBarData(data)

        // Set x-axis categories (dates)
        chartCategories = chartData.dates
        barSetModel = chartData.barSets

        console.log("Axis Categories (Dates for last 7 days):", chartCategories)

        if (mySeries.barSets && mySeries.barSets.length > 0) {
            for (var i = mySeries.barSets.length - 1; i >= 0; i--) {
                mySeries.remove(mySeries.barSets[i])
            }
        }

        for (var k = 0; k < barSetModel.length; k++) {
            let item = barSetModel[k];
            console.log("BarSet Label:", item.label);
            console.log("BarSet Values:", item.values);
            mySeries.append(item.label, item.values);
        }

        var maxVal = 0;
        for (var d = 0; d < chartCategories.length; d++) {
            var cumulative = 0;
            for (var b = 0; b < barSetModel.length; b++) {
                cumulative += barSetModel[b].values[d];
            }
            if (cumulative > maxVal) {
                maxVal = cumulative;
            }
        }
        valueAxis.max = maxVal * 1;
        console.log("Dynamic Y-Axis Max Value:", valueAxis.max);
        chartView.zoomReset();
    }

    Connections {
        target: Monitoring
        function onCriticalChangeDetected(message) {
            addCriticalChange(message)
        }
    }

    // ----------------------------------------------------------------
    // The Main UI
    // ----------------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10

        Label {
            text: "Windows Registry Monitor"
            font.pixelSize: 20
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Qt.AlignHCenter
            color: "darkBlue"
        }

        // The main tab bar
        TabBar {
            id: mainTabBar
            Layout.fillWidth: true

            TabButton { text: "Monitoring" }
            TabButton { text: "Logs" }
            TabButton { text: "Alert Settings" }
            TabButton { text: "Registry Keys" }
            TabButton { text: "Search" }
            TabButton { text: "Charts" }
        }

        // StackLayout for pages
        StackLayout {
            currentIndex: mainTabBar.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            // ---------------------------------------------------------
            // 1) Monitoring Page
            // ---------------------------------------------------------
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    id: monitoringColumn
                    anchors.top: parent.top
                    anchors.topMargin: 20
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 10

                    // Row for status + buttons
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 10

                        Label {
                            text: "Status: " + monitoringStatus
                            font.pixelSize: 16
                            color: "darkBlue"
                        }

                        Button {
                            text: "Start Monitoring"
                            onClicked: {
                                Monitoring.startMonitoring()
                                monitoringStatus = "Monitoring..."
                                addLog("[INFO] Monitoring started.")
                            }
                        }

                        Button {
                            text: "Stop Monitoring"
                            onClicked: {
                                Monitoring.stopMonitoring()
                                monitoringStatus = "Waiting..."
                                addLog("[INFO] Monitoring stopped.")
                            }
                        }
                    }

                    GroupBox {
                        title: "Critical Changes"
                        Layout.preferredWidth: 850
                        Layout.preferredHeight: 200
                        font.pointSize: 10

                        ScrollView {
                            width: parent.width
                            height: parent.height

                            ListView {
                                id: criticalChangesListView
                                width: parent.width
                                model: criticalChanges.length > 0 ? criticalChanges : [ { "changeText": "No changes detected yet." } ]

                                delegate: Rectangle {
                                    width: ListView.view ? ListView.view.width : 0
                                    height: 40
                                    color: "#F7F7F7"

                                    Row {
                                        width: parent.width
                                        spacing: 10
                                        anchors.verticalCenter: parent.verticalCenter

                                        Text {
                                            text: modelData.changeText.split('\\').pop() || ""
                                            font.pointSize: 10
                                            verticalAlignment: Text.AlignVCenter
                                            elide: Text.ElideRight
                                            width: parent.width * 0.6
                                        }

                                        Button {
                                            text: "Approve"
                                            visible: modelData.changeText !== "No changes detected yet."
                                            width: parent.width * 0.3
                                            onClicked: {
                                                let keyName = modelData.changeText.split("key: ")[1]
                                                if (keyName) {
                                                    Monitoring.allowChange(keyName.trim())
                                                }
                                                addLog("[INFO] Approved change for: " + keyName)
                                                for (let i = 0; i < criticalChanges.length; i++) {
                                                    if (criticalChanges[i].changeText === modelData.changeText) {
                                                        criticalChanges.splice(i, 1)
                                                        break
                                                    }
                                                }
                                                if (criticalChanges.length === 0) {
                                                    criticalChanges.push({ "changeText": "No changes detected yet." })
                                                }
                                                criticalChangesListView.model = criticalChanges
                                            }
                                        }
                                    }
                                }
                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                }
                            }
                        }
                    }
                }
            }

            // ---------------------------------------------------------
            // 2) Logs Page
            // ---------------------------------------------------------
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    id: logsColumn
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter

                    GroupBox {
                        title: "Logs"
                        Layout.preferredWidth: 600
                        Layout.preferredHeight: 300
                        font.pointSize: 10

                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 10

                            TextArea {
                                id: logArea
                                width: parent.width
                                height: 100
                                readOnly: true
                                placeholderText: "[INFO] System monitoring initialized."
                            }
                        }
                    }
                }
            }

            // ---------------------------------------------------------
            // 3) Alert Settings Page
            // ---------------------------------------------------------
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    id: userInfoColumn
                    anchors.top: parent.top
                    anchors.topMargin: 20
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 10

                    GroupBox {
                        title: "User Information"
                        Layout.preferredWidth: 600
                        Layout.preferredHeight: 180
                        font.pointSize: 10

                        ColumnLayout {
                            spacing: 10

                            TextField {
                                id: emailField
                                placeholderText: "Enter your email"
                                Layout.preferredWidth: 400
                            }

                            TextField {
                                id: phoneField
                                placeholderText: "Enter your phone number"
                                Layout.preferredWidth: 400
                            }
                        }
                    }

                    GroupBox {
                        title: "Alert Settings"
                        Layout.preferredWidth: 600
                        Layout.preferredHeight: 100
                        font.pointSize: 10

                        ColumnLayout {
                            spacing: 10

                            TextField {
                                id: nonCriticalAlertField
                                placeholderText: "Enter times before sending alert for non-critical changes."
                                Layout.preferredWidth: 400
                            }
                        }
                    }

                    GroupBox {
                        title: "Alert Frequency Settings"
                        Layout.preferredWidth: 600
                        Layout.preferredHeight: 100
                        font.pointSize: 10

                        ColumnLayout {
                            spacing: 10

                            ComboBox {
                                id: frequencyComboBox
                                width: 120
                                // You can use either a JS array or a ListModel here
                                model: ["Never", "1", "2", "5", "10", "15", "20", "30", "60"]

                                // Custom delegate for the popup items
                                delegate: ItemDelegate {
                                    width: frequencyComboBox.width
                                    text: modelData
                                    padding: 8
                                }
                            }
                        }
                    }

                    GroupBox {
                        title: "Save Settings"
                        Layout.preferredWidth: 600
                        Layout.preferredHeight: 100
                        font.pointSize: 10

                        ColumnLayout {
                            spacing: 10

                            Button {
                                text: "Save Settings"
                                width: 150
                                Layout.alignment: Qt.AlignHCenter
                                onClicked: {
                                    // Assign properties (assuming Settings is a context property)
                                    Settings.notificationFrequency = frequencyComboBox.currentText;
                                    Settings.email = emailField.text;
                                    Settings.phoneNumber = phoneField.text;
                                    Settings.nonCriticalAlertThreshold = nonCriticalAlertField.text;

                                    // Persist the settings to the database.
                                    Settings.saveSettings();

                                    Settings.settingsSaved.connect(function(success) {
                                        if (success) {
                                            console.log("[INFO] Settings saved to backend.");
                                        } else {
                                            console.error("[ERROR] Failed to save settings to backend.");
                                        }
                                    });
                                }
                            }
                        }
                    }
                }
            }

            // ---------------------------------------------------------
            // 4) Registry Keys Page (Improved Look)
            // ---------------------------------------------------------
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    id: registryKeysColumn
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: 20
                    spacing: 20

                    GroupBox {
                        title: "Registry Keys"
                        font.pointSize: 10
                        Layout.preferredWidth: 700
                        Layout.preferredHeight: 500
                        Layout.alignment: Qt.AlignHCenter

                        ScrollView {
                            anchors.fill: parent
                            clip: true

                            ListView {
                                id: keyListView
                                Layout.fillWidth: true
                                focus: true
                                model: Monitoring ? Monitoring.registryKeys : []

                                delegate: Rectangle {
                                    id: keyDelegate
                                    width: parent.width - 20
                                    height: 70
                                    radius: 8
                                    border.width: 1
                                    border.color: "#E0E0E0"
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.margins: 10

                                    Row {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 10

                                        CheckBox {
                                            id: keyCheckBox
                                            checked: model.isCritical
                                            // When toggled, update the status and log the change.
                                            onCheckedChanged: {
                                                Monitoring.setKeyCriticalStatus(model.name, checked)
                                                addLog("[INFO] " + model.name +
                                                       (checked ? " marked as critical." : " marked as uncritical."))
                                            }
                                        }

                                        Column {
                                            anchors.verticalCenter: parent.verticalCenter
                                            spacing: 2

                                            // Main text (e.g., title)
                                            Text {
                                                text: model.displayText
                                                font.pixelSize: 16
                                                font.bold: true
                                                color: "#333"
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }

                                    // MouseArea covers the entire delegate to allow clicking anywhere to toggle the CheckBox.
                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: keyCheckBox.checked = !keyCheckBox.checked
                                        onEntered: keyDelegate.color = "#CFD8DC"
                                        onExited: keyDelegate.color = index % 2 === 0 ? "#FFFFFF" : "#F5F5F5"
                                    }
                                }

                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                }
                            }
                        }
                    }
                }
            }


            // 5) Search Page
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.top: parent.top
                    anchors.topMargin: 20
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 10

                    // 5a) Date Range
                    GroupBox {
                        title: "Search by Date"
                        Layout.preferredWidth: 700
                        Layout.preferredHeight: 80
                        font.pointSize: 10
                        Row {
                            spacing: 10
                            TextField {
                                id: searchStartDateField
                                placeholderText: "Start date (YYYY-MM-DD)"
                                width: 200; height: 30
                                onEditingFinished: updateSearchResults()
                            }
                            TextField {
                                id: searchEndDateField
                                placeholderText: "End date (YYYY-MM-DD)"
                                width: 200; height: 30
                                onEditingFinished: updateSearchResults()
                            }
                        }
                    }

                    // 5b) Config Name
                    GroupBox {
                        title: "Search by Name"
                        Layout.preferredWidth: 700
                        Layout.preferredHeight: 80
                        font.pointSize: 10
                        Row {
                            spacing: 10
                            TextField {
                                id: searchKeyNameField
                                placeholderText: "Enter key name"
                                width: 200; height: 30
                                onEditingFinished: updateSearchResults()
                            }
                        }
                    }

                    // 5c) Filters (using CheckBoxes)
                    GroupBox {
                        title: "Filter Options"
                        Layout.preferredWidth: 700
                        Layout.preferredHeight: 80
                        font.pointSize: 10
                        Row {
                            spacing: 10
                            CheckBox {
                                id: acknowledgedOnlyCheckBox
                                topPadding: 10
                                text: "Only approved"
                                onClicked: {
                                    console.log("Only approved checked:", checked);
                                    if (checked) {
                                        unacknowledgedOnlyCheckBox.checked = false;
                                    }
                                    updateSearchResults();
                                }
                            }
                            CheckBox {
                                id: unacknowledgedOnlyCheckBox
                                topPadding: 10
                                text: "Only disapproved"
                                onClicked: {
                                    console.log("Only disapproved checked:", checked);
                                    if (checked) {
                                        acknowledgedOnlyCheckBox.checked = false;
                                    }
                                    updateSearchResults();
                                }
                            }
                            CheckBox {
                                id: criticalOnlyCheckBox
                                topPadding: 10
                                text: "Only critical"
                                onClicked: {
                                    console.log("Only critical checked:", checked);
                                    if (checked) {
                                        nonCriticalOnlyCheckBox.checked = false;
                                    }
                                    updateSearchResults();
                                }
                            }
                            CheckBox {
                                id: nonCriticalOnlyCheckBox
                                topPadding: 10
                                text: "Only non-critical"
                                onClicked: {
                                    console.log("Only non-critical checked:", checked);
                                    if (checked) {
                                        criticalOnlyCheckBox.checked = false;
                                    }
                                    updateSearchResults();
                                }
                            }
                        }
                    }

                    // 5d) Action Buttons
                    GroupBox {
                        title: "Search / Clear"
                        Layout.preferredWidth: 700
                        Layout.preferredHeight: 80
                        font.pointSize: 10
                        Row {
                            spacing: 10
                            Button {
                                text: "Search"
                                height: 35
                                onClicked: {
                                    updateSearchResults();
                                }
                            }
                            Button {
                                text: "Clear"
                                height: 35
                                onClicked: {
                                    searchStartDateField.text = "";
                                    searchEndDateField.text = "";
                                    searchKeyNameField.text = "";
                                    acknowledgedOnlyCheckBox.checked = false;
                                    unacknowledgedOnlyCheckBox.checked = false;
                                    criticalOnlyCheckBox.checked = false;
                                    nonCriticalOnlyCheckBox.checked = false;
                                    window.searchResults = [];
                                }
                            }
                        }
                    }

                    // 5e) Search Results Display
                    GroupBox {
                        title: "Search Results"
                        Layout.preferredWidth: 700
                        Layout.preferredHeight: 170
                        font.pointSize: 10
                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            ScrollView {
                                width: parent.width
                                height: 100
                                TextArea {
                                    id: searchResultsArea
                                    width: parent.width
                                    height: parent.height
                                    readOnly: true
                                    font.pointSize: 10
                                    wrapMode: TextArea.NoWrap
                                    placeholderText: "No search performed."
                                    text: (window.searchResults && window.searchResults.length > 0)
                                          ? formatSearchResults(window.searchResults)
                                          : "No search performed."
                                }
                            }
                        }
                    }
                }
            }

            // ---------------------------------------------------------
            // 6) Charts Page
            // ---------------------------------------------------------
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ChartView {
                    id: chartView
                    anchors.fill: parent
                    title: "Stacked Bar Series"
                    legend.alignment: Qt.AlignBottom
                    antialiasing: true

                    BarCategoryAxis {
                        id: categoryAxisX
                        categories: chartCategories
                    }

                    ValueAxis {
                        id: valueAxis
                        min: 0
                        max: valueAxis.max * 1.1
                    }

                    StackedBarSeries {
                        id: mySeries
                        axisX: categoryAxisX
                        axisY: valueAxis
                    }
                }
            }
        }
    }
}
