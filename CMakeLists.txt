cmake_minimum_required(VERSION 3.16)

project(Monitor VERSION 0.1 LANGUAGES CXX)

add_definitions(-DAWS_MSVC_CLZ_U32_DEFINED)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#-----------------------------------------------------------------------------
# 1) Define the Qt & AWS path so CMake can find them on each platform
#-----------------------------------------------------------------------------
if (APPLE)
    # Homebrew-installed Qt + AWS SDK
    set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt;/opt/homebrew/opt/aws-sdk-cpp")

    # MySQL Connector/C++
    set(MYSQL_CONNECTOR_PATH "/opt/homebrew/Cellar/mysql-connector-c++/9.2.0")
    set(MYSQL_INCLUDE_DIR "${MYSQL_CONNECTOR_PATH}/include")
    set(MYSQL_LIB_DIR     "${MYSQL_CONNECTOR_PATH}/lib")
    set(MYSQL_LIBRARIES
        "${MYSQL_LIB_DIR}/libmysqlcppconnx.dylib"  # X Dev API lib
    )
elseif (WIN32)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.8.2/msvc2022_64/lib/cmake;C:/SDKs/aws-sdk-cpp-build/aws-sdk-cpp-install")

    set(MYSQL_CONNECTOR_PATH "C:/MySQL/mysql-connector-c++-9.1.0-winx64")
    set(MYSQL_INCLUDE_DIR    "${MYSQL_CONNECTOR_PATH}/include")
    set(MYSQL_LIB_DIR        "${MYSQL_CONNECTOR_PATH}/lib64/vs14") # or vs16, etc.

    set(MYSQL_LIBRARIES
        "${MYSQL_LIB_DIR}/mysqlcppconn.lib"
        "${MYSQL_LIB_DIR}/mysqlcppconn-static.lib"
    )
else()
    message(FATAL_ERROR "Unsupported platform! Please extend CMakeLists for your OS.")
endif()

#-----------------------------------------------------------------------------
# 2) Find Qt (Quick + Sql)
#-----------------------------------------------------------------------------
find_package(Qt6 6.5 REQUIRED COMPONENTS Test Quick Sql Charts)
qt_standard_project_setup(REQUIRES 6.5)

#-----------------------------------------------------------------------------
# 3) Find AWS SDK (sns, sesv2), OpenSSL
#-----------------------------------------------------------------------------
find_package(AWSSDK REQUIRED COMPONENTS sns sesv2)
find_package(OpenSSL REQUIRED)

#-----------------------------------------------------------------------------
# 4) Define source files
#-----------------------------------------------------------------------------
set(HEADER_FILES
    include/registryKey.h
    include/MacOSMonitoring.h
    include/WindowsMonitoring.h
    include/WindowsJsonUtils.h
    include/MacOSRollback.h
    include/WindowsRollback.h
    include/registryKeyModel.h
    include/alert.h
    include/settings.h
    include/plistFile.h
    include/plistFileModel.h
    include/monitoringBase.h
    include/Database.h
    include/encryptionUtils.h
)

set(SOURCE_FILES
    src/main.cpp
    src/registryKey.cpp
    src/MacOSMonitoring.cpp
    src/WindowsJsonUtils.cpp
    src/MacOSRollback.cpp
    src/WindowsRollback.cpp
    src/MacOSJsonUtils.cpp
    src/WindowsMonitoring.cpp
    src/registryKeyModel.cpp
    src/alert.cpp
    src/settings.cpp
    src/plistFile.cpp
    src/plistFileModel.cpp
    src/Database.cpp
    src/encryptionUtils.cpp
)

# Group them in IDEs like Visual Studio
source_group("Header Files" FILES ${HEADER_FILES})
source_group("Source Files" FILES ${SOURCE_FILES})

#-----------------------------------------------------------------------------
# 5) Create the executable (Qt wrapper function for C++ apps)
#-----------------------------------------------------------------------------
qt_add_executable(appMonitor
    ${HEADER_FILES}
    ${SOURCE_FILES}
)

#-----------------------------------------------------------------------------
# 6) Add include directories for local headers + MySQL + OpenSSL
#-----------------------------------------------------------------------------
target_include_directories(appMonitor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include    # <-- Local "include" folder
        ${MYSQL_INCLUDE_DIR}                   # MySQL
        ${OPENSSL_INCLUDE_DIR}                 # OpenSSL
)

#-----------------------------------------------------------------------------
# 7) Link libraries: OpenSSL, Qt, AWS, (Optional) MySQL
#-----------------------------------------------------------------------------
target_link_libraries(appMonitor PRIVATE
    ${OPENSSL_LIBRARIES}     # For encryption
    Qt6::Quick               # Qt QML/Quick
    Qt6::Sql                 # Qt SQL
    Qt6::Core
    Qt6::Charts
    Qt6::Test
    ${AWSSDK_LINK_LIBRARIES} # AWS (SNS, SESv2, etc.)
)

#-----------------------------------------------------------------------------
# 8) Add a QML module with .qml files
#-----------------------------------------------------------------------------
qt_add_qml_module(appMonitor
    URI Monitor
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Welcome.qml
        qml/WindowsDashboard.qml
        qml/MacOSDashboard.qml
)

#-----------------------------------------------------------------------------
# 9) Platform bundle settings (macOS or Windows)
#-----------------------------------------------------------------------------
set_target_properties(appMonitor PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

#-----------------------------------------------------------------------------
# 10) Install rules for the executable and libraries
#-----------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS appMonitor
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

#-----------------------------------------------------------------------------
# 11) Install JSON files as runtime resources
#-----------------------------------------------------------------------------
set(JSON_FILES
    resources/monitoredPlists.json
    resources/awsconfig.json
    resources/monitoredKeys.json
    resources/encryptionKeys.json
)

install(FILES ${JSON_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/resources)
