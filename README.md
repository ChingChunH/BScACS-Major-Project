# BScACS-Major-Project
Dual-Platform System Configuration Monitor

### COMP 8800 & 8900 Major Project

A cross-platform tool that continuously watches Windows registry keys and macOS `.plist` files
for changes, rolls back unauthorized critical modifications, and issues configurable alerts 
via AWS SES (email) and SNS (SMS). All events, user preferences, and alert history are securely 
logged to a MySQL database, with sensitive fields encrypted via OpenSSL.

## Table of Contents
1. [Key Features](#key_Features)
2. [Prerequisites](#prerequisites)
3. [Installation & Build](#installation--build)
4. [Configuration](#configuration)
5. [Usage](#usage)
6. [Project Structure](#project-structure)

## Key Features
- **Real-time Monitoring**<br />
    - Windows: watches registry keys (e.g. DoubleClickSpeed, CursorBlinkRate).<br />
    - macOS: watches .plist entries under /Library/Preferences or any JSON-listed path.<br />
    
- **Critical-Change Rollback**<br />
  - Automatically reverts any change to a “critical” key or plist entry.<br />
    
- **Custom Alerts**<br />
    - Sends email and/or SMS via AWS SES & SNS for critical changes.<br />
    - Non-critical changes trigger alerts after a user-defined count threshold.<br />
    - Rate-limits alerts per hour to avoid spam. <br />
    
- **Secure Persistence**<br />
    - MySQL database logs every change, user settings, and alert history.<br />
    - Sensitive data (emails, phone numbers, config values) encrypted with AES-256-CBC via OpenSSL.<br />

- **Interactive UI (Qt/QML)**<br />
    - Start/stop monitoring, view live logs, configure thresholds and frequency. <br />
    - Browse monitored items, toggle critical status, search history, and view a 7-day bar chart.<br />

## Prerequisites
### Common
- Qt 6.8.2+ with Qt Quick/QML & Qt Charts
- CMake (>= 3.16)
- AWS SDK for C++
- MySQL SErver + Connector/C++
- OpenSSL (for encryption)

### Winwdows
- Windows 10 or later (64-bit)
- Registry APIs via QSettings::NativeFormat

###macOS
- macOS 11.0 or later
- `.plist` montioring via `QFileSystemWatcher` & `QSettings::NativeFormat`

## Installation & Build
1. Clone the repository
   ```
   git clone https://github.com/ChingChunH/BScACS-Major-Project.git
   ```
2. Install dependencies
   - Build and install the AWS SDK for C++
   - Ensure MySQL Server is running and you have a user with privileges
   - Confirm OpenSSL headers/libraries are available
     
3. Configure AWS SDK path
   - Edit `CMakeLists.txt with your AWS SDK install path`
  
5. Generate & Build
   ```
   mkdir build && cd build
   cmake ..
   cmake --build .
   ```
## Configuration
1. AWS Credentials
   Plase `awsconfig.json in `resources/`
   ```
   {
    "accessKeyId":     "YOUR_ACCESS_KEY_ID",
    "secretAccessKey": "YOUR_SECRET_ACCESS_KEY",
    "region":          "YOUR_AWS_REGION"
    }
    ```
2. Encrption Keys
    ```
     {
        "key": "BASE64_ENCODED_32_BYTE_AES_KEY",
        "iv":  "BASE64_ENCODED_16_BYTE_AES_IV"
     }
    ```
3. Monitored Items
   - **Windows**: `resources/monitoredKeys.json`
   - **macOS**: `resources/monitoredPlists.json`<br />
     - Windows example:<br />
     ```
     [
      {
        "hive": "HKEY_CURRENT_USER",
        "keyPath": "Control Panel\\Mouse",
        "valueName": "DoubleClickSpeed",
        "isCritical": true
      },
      {
        "hive": "HKEY_CURRENT_USER",
        "keyPath": "Control Panel\\Keyboard",
        "valueName": "CursorBlinkRate",
        "isCritical": false
      }
      ]
      ```
      - macOS example:<br />
      ```
      [
        {
          "plistPath": "~/Library/Preferences/com.apple.dock.plist",
          "valueName": "mineffect",
          "isCritical": false
        }
      ]
      ```
4. Database
   - MySQL connection settings are in the `Database.cpp` (`host`, `port`, `user`, `password`)
   - On startup, the app will create the `MonitorDB` database and tables (`UserSettings`, `ConfigurationSettings`, `Changes`) if they don't exist.

## Usage
1. Launch the Qt application.<br />

2. Monitoring tab
   - Start/Stop monitoring
   - View live "Critical Changes" list; Acknowledge to cancel rollback.<br />
   
3. Logs tab
   - Scroll through detailed change and alert logs.<br />

4. Alert Settings
   - Enter email & phone, set non-critical threshold & frequency, then Save.<br />
   
5. Monitored Items
   - Toggle "Critical" flag for any key/plist entry; chnage persist.<br />
   
6. Search
    - Qeury historical changes by date range, key/name, and acknowledged/critical<.br />
    
7. Charts
    - View a 7-day stacked bar chart of change counts by configuration.<br />
  
## Project Structure
/src<br />
  ├─ alert.{h,cpp}<br />
  ├─ Database.{h,cpp}<br />
  ├─ EncryptionUtils.{h,cpp}<br />
  ├─ MacOSMonitoring.{h,cpp}, WindowsMonitoring.{h,cpp}<br />
  ├─ MacOSRollback.{h,cpp}, WindowsRollback.{h,cpp}<br />
  ├─ PlistFile.*, RegistryKey.*, *Model.*<br />
  ├─ WindowsJsonUtils.{h,cpp}, MacOSJsonUtils.{h,cpp}<br />
  └─ main.cpp<br />
<br />
/resources<br />
  ├─ monitoredKeys.json<br />
  ├─ monitoredPlists.json<br />
  ├─ awsconfig.json<br />
  └─ encryptionKeys.json<br />
<br />
/qml<br />
  └─ Main.qml    # UI definition (tabs: Monitoring, Logs, Settings, etc.)<br />
<br />
CMakeLists.txt
