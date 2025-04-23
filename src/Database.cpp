#include "Database.h"
#include "EncryptionUtils.h"

#include <QDir>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QRegularExpression>

// Static flag to ensure we only create the MonitorDB database once per process
static bool s_databaseInitialized = false;

////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Construct the Database object.
 *
 * - Ensures the MySQL database MonitorDB exists (creates it if not).
 * - Opens a persistent connection to MonitorDB.
 * - Creates required tables if they do not exist.
 * - Loads encryption keys for later use.
 */
Database::Database(QObject *parent)
    : QObject(parent)
{
    // One-time creation of the MonitorDB database itself
    if (!s_databaseInitialized) {
        {
            // Temporary connection with no default DB to check/create MonitorDB
            QSqlDatabase tempDb = QSqlDatabase::addDatabase("QMYSQL", "temp_connection");
            tempDb.setHostName("localhost");
            tempDb.setPort(3306);
            tempDb.setDatabaseName("");
            tempDb.setUserName("monitor_user");
            tempDb.setPassword("Monitor1230.");

            if (!tempDb.open()) {
                qWarning() << "[DATABASE] Failed to open temporary connection:"
                           << tempDb.lastError().text();
            } else {
                // Check if MonitorDB exists
                QSqlQuery checkQuery(tempDb);
                if (checkQuery.exec("SHOW DATABASES LIKE 'MonitorDB'")) {
                    if (checkQuery.next()) {
                        qDebug() << "[DATABASE] Database MonitorDB already exists.";
                    } else {
                        // Create MonitorDB if missing
                        QSqlQuery createQuery(tempDb);
                        if (!createQuery.exec("CREATE DATABASE MonitorDB")) {
                            qWarning() << "[DATABASE] Failed to create MonitorDB:"
                                       << createQuery.lastError().text();
                        } else {
                            qDebug() << "[DATABASE] Database MonitorDB created successfully.";
                        }
                    }
                } else {
                    qWarning() << "[DATABASE] Failed to check for database existence:"
                               << checkQuery.lastError().text();
                }
                tempDb.close();
            }
        }
        QSqlDatabase::removeDatabase("temp_connection");
        s_databaseInitialized = true;
    }

    // Establish or reuse the default Qt SQL connection to MonitorDB
    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        db = QSqlDatabase::database("qt_sql_default_connection");
    } else {
        db = QSqlDatabase::addDatabase("QMYSQL", "qt_sql_default_connection");
        db.setHostName("localhost");
        db.setPort(3306);
        db.setDatabaseName("MonitorDB");
        db.setUserName("monitor_user");
        db.setPassword("Monitor1230.");
    }

    static bool s_dbConnectionLogged = false;
    if (!db.isOpen() && !db.open()) {
        qWarning() << "[DATABASE] Failed to connect to MonitorDB:"
                   << db.lastError().text();
    } else {
        if (!s_dbConnectionLogged) {
            qDebug() << "[DATABASE] Database connection established.";
            s_dbConnectionLogged = true;
        }
        // Ensure all required tables exist
        createSchema();
    }

    // Load encryption keys for encrypting/decrypting sensitive fields
    static bool s_keysLoadedLogged = false;
    QString encryptionKeysPath = resolveEncryptionKeysPath();
    EncryptionUtils::loadEncryptionKeys(encryptionKeysPath);
    if (!s_keysLoadedLogged) {
        qDebug() << "[DATABASE] Encryption keys loaded from:" << encryptionKeysPath;
        s_keysLoadedLogged = true;
    }
}

/**
 * @brief Destructor closes the database connection if open.
 */
Database::~Database() {
    if (db.isOpen()) {
        db.close();
    }
}

////////////////////////////////////////////////////////////////////////////////
// Path Resolution
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Determine the path to the encryption keys JSON file.
 * @return Absolute path depending on platform (macOS vs. others).
 */
QString Database::resolveEncryptionKeysPath() {
#ifdef Q_OS_MAC
    return QDir::cleanPath(
        QCoreApplication::applicationDirPath() +
        "/../../../../../resources/encryptionKeys.json"
        );
#else
    return QDir::cleanPath(
        QCoreApplication::applicationDirPath() +
        "/../../resources/encryptionKeys.json"
        );
#endif
}

////////////////////////////////////////////////////////////////////////////////
// Connection Management
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Ensure the database connection is open, reopening if necessary.
 */
void Database::ensureConnection() {
    if (!db.isOpen()) {
        qWarning() << "[DATABASE] Connection closed; attempting reopen...";
        if (!db.open()) {
            qCritical() << "[DATABASE] Failed to reopen connection:"
                        << db.lastError().text();
        } else {
            qDebug() << "[DATABASE] Connection reopened successfully.";
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Schema Creation
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Create tables UserSettings, ConfigurationSettings, and Changes if missing.
 * @return True if schema exists or was created successfully.
 */
bool Database::createSchema() {
    static bool s_schemaCreated = false;
    if (s_schemaCreated) {
        return true;
    }

    QSqlQuery query;

    // ── UserSettings table ─────────────────────────────────────────────
    query.exec("SHOW TABLES LIKE 'UserSettings'");
    if (!query.next()) {
        QString sql = R"(
            CREATE TABLE UserSettings (
                id INT AUTO_INCREMENT PRIMARY KEY,
                user_email VARCHAR(512) DEFAULT '',
                phone_number VARCHAR(512) DEFAULT '',
                non_critical_threshold INT DEFAULT 0,
                notification_frequency VARCHAR(255) DEFAULT 'Never',
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                UNIQUE KEY unique_email (user_email),
                UNIQUE KEY unique_phone (phone_number)
            )
        )";
        if (!query.exec(sql)) {
            qWarning() << "[DATABASE] Failed to create UserSettings table:"
                       << query.lastError().text();
            return false;
        }
        qDebug() << "[DATABASE] UserSettings table created.";
    } else {
        qDebug() << "[DATABASE] UserSettings table already exists.";
    }

    // ── ConfigurationSettings table ─────────────────────────────────
    query.exec("SHOW TABLES LIKE 'ConfigurationSettings'");
    if (!query.next()) {
        QString sql = R"(
            CREATE TABLE ConfigurationSettings (
                id INT AUTO_INCREMENT PRIMARY KEY,
                config_name VARCHAR(255) UNIQUE,
                config_path VARCHAR(512),
                config_value TEXT,
                is_critical BOOLEAN DEFAULT FALSE,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        )";
        if (!query.exec(sql)) {
            qWarning() << "[DATABASE] Failed to create ConfigurationSettings:"
                       << query.lastError().text();
            return false;
        }
        qDebug() << "[DATABASE] ConfigurationSettings table created.";
    } else {
        qDebug() << "[DATABASE] ConfigurationSettings table already exists.";
    }

    // ── Changes table ─────────────────────────────────────────────────
    query.exec("SHOW TABLES LIKE 'Changes'");
    if (!query.next()) {
        QString sql = R"(
            CREATE TABLE Changes (
                id INT AUTO_INCREMENT PRIMARY KEY,
                config_name VARCHAR(255),
                old_value TEXT,
                new_value TEXT,
                acknowledged BOOLEAN DEFAULT FALSE,
                critical BOOLEAN DEFAULT FALSE,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        )";
        if (!query.exec(sql)) {
            qWarning() << "[DATABASE] Failed to create Changes table:"
                       << query.lastError().text();
            return false;
        }
        qDebug() << "[DATABASE] Changes table created.";
    } else {
        qDebug() << "[DATABASE] Changes table already exists.";
    }

    s_schemaCreated = true;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// User Settings CRUD
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Insert new user settings row (email, phone, threshold, frequency).
 * @return True on success.
 */
bool Database::insertUserSettings(const QString &email,
                                  const QString &phone,
                                  int threshold,
                                  const QString &notificationFrequency)
{
    ensureConnection();

    if (email.isEmpty() && phone.isEmpty()) {
        qWarning() << "[DATABASE] Empty email & phone; skipping insert.";
        return false;
    }

    qDebug() << "[DATABASE] Inserting user settings:"
             << "email=" << email
             << "phone=" << phone
             << "threshold=" << threshold
             << "freq=" << notificationFrequency;

    // Encrypt sensitive fields before storing
    QByteArray encryptedEmail, encryptedPhone;
    if (!email.isEmpty()) {
        encryptedEmail = EncryptionUtils::encrypt(email);
    }
    if (!phone.isEmpty()) {
        encryptedPhone = EncryptionUtils::encrypt(phone);
    }

    QSqlQuery insertQuery;
    insertQuery.prepare(R"(
        INSERT INTO UserSettings
          (user_email, phone_number, non_critical_threshold, notification_frequency)
        VALUES
          (:email, :phone, :threshold, :frequency)
    )");
    insertQuery.bindValue(":email",
                          email.isEmpty() ? QVariant(QVariant::String)
                                          : encryptedEmail.toBase64());
    insertQuery.bindValue(":phone",
                          phone.isEmpty() ? QVariant(QVariant::String)
                                          : encryptedPhone.toBase64());
    insertQuery.bindValue(":threshold", threshold);
    insertQuery.bindValue(":frequency", notificationFrequency);

    if (!insertQuery.exec()) {
        qWarning() << "[DATABASE] Failed to insert user settings:"
                   << insertQuery.lastError().text();
        return false;
    }

    qDebug() << "[DATABASE] User settings inserted.";
    return true;
}

// Helper functions to validate input format
static bool isValidEmail(const QString &email) {
    QRegularExpression re(R"((^[\w\.\-]+)@([\w\-]+)((\.(\w){2,3})+)$)");
    return re.match(email).hasMatch();
}
static bool isValidPhoneNumber(const QString &phone) {
    QRegularExpression re(R"(^[+\-\d\s]+$)");
    return re.match(phone).hasMatch();
}

/**
 * @brief Insert or update user settings; validates email/phone first.
 * @return True on success.
 */
bool Database::insertOrUpdateUserSettings(const QString &email,
                                          const QString &phone,
                                          int threshold,
                                          const QString &notificationFrequency)
{
    ensureConnection();

    // Validate formats
    if (!email.isEmpty() && !isValidEmail(email)) {
        qWarning() << "[DATABASE] Invalid email:" << email;
        return false;
    }
    if (!phone.isEmpty() && !isValidPhoneNumber(phone)) {
        qWarning() << "[DATABASE] Invalid phone:" << phone;
        return false;
    }
    if (email.isEmpty() && phone.isEmpty()) {
        qWarning() << "[DATABASE] Empty email & phone; skipping.";
        return false;
    }

    qDebug() << "[DATABASE] Insert/update user settings:"
             << "email=" << email
             << "phone=" << phone
             << "threshold=" << threshold
             << "freq=" << notificationFrequency;

    QByteArray encryptedEmail = email.isEmpty() ? QByteArray() : EncryptionUtils::encrypt(email);
    QByteArray encryptedPhone = phone.isEmpty() ? QByteArray() : EncryptionUtils::encrypt(phone);

    // Check if an entry already exists by email or phone
    QSqlQuery checkQuery;
    bool exists = false;
    int existingId = -1;
    if (!email.isEmpty()) {
        checkQuery.prepare("SELECT id FROM UserSettings WHERE user_email = :email");
        checkQuery.bindValue(":email", encryptedEmail.toBase64());
    } else {
        checkQuery.prepare("SELECT id FROM UserSettings WHERE phone_number = :phone");
        checkQuery.bindValue(":phone", encryptedPhone.toBase64());
    }
    if (checkQuery.exec() && checkQuery.next()) {
        exists = true;
        existingId = checkQuery.value("id").toInt();
    }

    if (exists) {
        // Update existing record
        QSqlQuery updateQuery;
        updateQuery.prepare(R"(
            UPDATE UserSettings
            SET phone_number = :phone,
                non_critical_threshold = :threshold,
                notification_frequency = :frequency,
                timestamp = CURRENT_TIMESTAMP
            WHERE id = :id
        )");
        updateQuery.bindValue(":phone",
                              phone.isEmpty() ? QVariant(QVariant::String)
                                              : encryptedPhone.toBase64());
        updateQuery.bindValue(":threshold", threshold);
        updateQuery.bindValue(":frequency", notificationFrequency);
        updateQuery.bindValue(":id", existingId);

        if (!updateQuery.exec()) {
            qWarning() << "[DATABASE] Failed to update user settings:"
                       << updateQuery.lastError().text();
            return false;
        }
        qDebug() << "[DATABASE] Updated user settings for id" << existingId;
    } else {
        // Insert a new record
        Database::insertUserSettings(email, phone, threshold, notificationFrequency);
    }

    return true;
}

/**
 * @brief Retrieve all user settings, decrypting email & phone.
 * @return List of maps containing id, email, phone, threshold, timestamp.
 */
QVariantList Database::getAllUserSettings() {
    ensureConnection();
    QVariantList list;
    QSqlQuery query("SELECT * FROM UserSettings");

    while (query.next()) {
        QVariantMap row;
        row["id"] = query.value("id");
        QByteArray encEmail = QByteArray::fromBase64(
            query.value("user_email").toByteArray()
            );
        QByteArray encPhone = QByteArray::fromBase64(
            query.value("phone_number").toByteArray()
            );
        row["email"]     = EncryptionUtils::decrypt(encEmail);
        row["phone"]     = EncryptionUtils::decrypt(encPhone);
        row["threshold"]= query.value("non_critical_threshold");
        row["timestamp"]= query.value("timestamp");
        list.append(row);
    }
    return list;
}

////////////////////////////////////////////////////////////////////////////////
// ConfigurationSettings CRUD
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Insert or update a configuration setting.
 * @return True on success.
 */
bool Database::insertOrUpdateConfiguration(const QString &configName,
                                           const QString &configPath,
                                           const QString &configValue,
                                           bool isCritical)
{
    ensureConnection();

    QByteArray encryptedValue = EncryptionUtils::encrypt(configValue);

    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO ConfigurationSettings
          (config_name, config_path, config_value, is_critical)
        VALUES
          (:configName, :configPath, :configValue, :isCritical)
        ON DUPLICATE KEY UPDATE
          config_path = VALUES(config_path),
          config_value = VALUES(config_value),
          is_critical = VALUES(is_critical)
    )");
    query.bindValue(":configName", configName);
    query.bindValue(":configPath", configPath);
    query.bindValue(":configValue", encryptedValue);
    query.bindValue(":isCritical", isCritical);

    if (!query.exec()) {
        qWarning() << "[DATABASE] Failed to upsert configuration:"
                   << query.lastError().text();
        return false;
    }
    return true;
}

/**
 * @brief Retrieve all configuration settings.
 * @return List of maps with id, name, path, value (encrypted), is_critical, timestamp.
 */
QVariantList Database::getAllConfigurations() {
    ensureConnection();
    QVariantList list;
    QSqlQuery query("SELECT * FROM ConfigurationSettings");

    while (query.next()) {
        QVariantMap row;
        row["id"]           = query.value("id");
        row["config_name"]  = query.value("config_name");
        row["config_path"]  = query.value("config_path");
        row["config_value"] = query.value("config_value");
        row["is_critical"]  = query.value("is_critical");
        row["timestamp"]    = query.value("timestamp");
        list.append(row);
    }
    return list;
}

////////////////////////////////////////////////////////////////////////////////
// Change Log CRUD
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Insert a change event into the Changes table.
 * @return True on success.
 */
bool Database::insertChange(const QString &configName,
                            const QString &oldValue,
                            const QString &newValue,
                            bool acknowledged,
                            bool critical)
{
    ensureConnection();

    QByteArray encOld = EncryptionUtils::encrypt(oldValue);
    QByteArray encNew = EncryptionUtils::encrypt(newValue);

    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO Changes
          (config_name, old_value, new_value, acknowledged, critical)
        VALUES
          (:configName, :oldValue, :newValue, :acknowledged, :critical)
    )");
    query.bindValue(":configName", configName);
    query.bindValue(":oldValue", encOld.toBase64());
    query.bindValue(":newValue", encNew.toBase64());
    query.bindValue(":acknowledged", acknowledged);
    query.bindValue(":critical", critical);

    if (!query.exec()) {
        qWarning() << "[DATABASE] Failed to insert change:"
                   << query.lastError().text();
        return false;
    }
    return true;
}

/**
 * @brief Retrieve all change records (unencrypted).
 * @return List of maps with id, config_name, old/new values (base64), acknowledged, timestamp.
 */
QVariantList Database::getAllChanges() {
    ensureConnection();
    QVariantList list;
    QSqlQuery query("SELECT * FROM Changes");

    while (query.next()) {
        QVariantMap row;
        row["id"]           = query.value("id");
        row["config_name"]  = query.value("config_name");
        row["old_value"]    = query.value("old_value").toString();
        row["new_value"]    = query.value("new_value").toString();
        row["acknowledged"] = query.value("acknowledged").toBool();
        row["timestamp"]    = query.value("timestamp");
        list.append(row);
    }
    return list;
}

/**
 * @brief Mark all unacknowledged changes for a config as acknowledged.
 * @return True if any rows were updated.
 */
bool Database::updateAcknowledgmentStatus(const QString &configName) {
    ensureConnection();

    QSqlQuery query;
    query.prepare(R"(
        UPDATE Changes
        SET acknowledged = TRUE
        WHERE config_name = :configName
          AND acknowledged = FALSE
    )");
    query.bindValue(":configName", configName);

    if (!query.exec()) {
        qWarning() << "[DATABASE] Failed to update acknowledgment status:"
                   << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

/**
 * @brief Search change history by exact date and/or config name.
 * @return Matching records list.
 */
QVariantList Database::searchChangeHistory(const QString &date,
                                           const QString &configName)
{
    ensureConnection();
    QVariantList results;
    QString sql = R"(
        SELECT id, config_name, old_value, new_value, timestamp
        FROM Changes
        WHERE 1=1
    )";
    if (!date.isEmpty()) {
        sql += " AND DATE(timestamp) = :date";
    }
    if (!configName.isEmpty()) {
        sql += " AND config_name = :configName";
    }

    QSqlQuery query;
    query.prepare(sql);
    if (!date.isEmpty())        query.bindValue(":date", date);
    if (!configName.isEmpty())  query.bindValue(":configName", configName);

    if (!query.exec()) {
        qWarning() << "[DATABASE] Search history failed:" << query.lastError().text();
        return results;
    }

    while (query.next()) {
        QVariantMap rec;
        rec["id"]           = query.value("id");
        rec["config_name"]  = query.value("config_name");
        rec["old_value"]    = query.value("old_value").toString();
        rec["new_value"]    = query.value("new_value").toString();
        rec["timestamp"]    = query.value("timestamp");
        results.append(rec);
    }
    return results;
}

/**
 * @brief Get change counts for each config over the past 7 days.
 * @return List of records with date, config_name, and count.
 */
QVariantList Database::getChangesCountByDateAndConfig() {
    ensureConnection();
    QVariantList results;

    QString sql = R"(
        SELECT DATE(timestamp) AS date,
               config_name,
               COUNT(*) AS change_count
        FROM Changes
        WHERE DATE(timestamp) >= CURDATE() - INTERVAL 6 DAY
        GROUP BY DATE(timestamp), config_name
        ORDER BY date ASC
    )";

    QSqlQuery query;
    if (!query.exec(sql)) {
        qWarning() << "[DATABASE] Failed to get change counts:" << query.lastError();
        return results;
    }

    while (query.next()) {
        QVariantMap row;
        row["date"]        = query.value("date").toString();
        row["config_name"] = query.value("config_name").toString();
        row["count"]       = query.value("change_count").toInt();
        results.append(row);
    }
    return results;
}

/**
 * @brief Search change history within a date range and optional filters.
 * @return Matching records list with all fields.
 */
QVariantList Database::searchChangeHistoryRange(const QString &start,
                                                const QString &end,
                                                const QString &configName,
                                                const QVariant &ackFilter,
                                                const QVariant &criticalFilter)
{
    ensureConnection();
    QVariantList results;
    if (!db.isOpen()) {
        qWarning() << "[DATABASE] Cannot search; DB is closed.";
        return results;
    }

    // Build dynamic SQL query with optional WHERE clauses
    QString sql = R"(
        SELECT id, config_name, old_value, new_value,
               acknowledged, critical, timestamp
        FROM Changes
        WHERE 1=1
    )";
    if (!start.isEmpty())         sql += " AND timestamp >= :start";
    if (!end.isEmpty())           sql += " AND timestamp <= :end";
    if (!configName.isEmpty())    sql += " AND config_name = :configName";
    if (!ackFilter.isNull())      sql += " AND acknowledged = :ackFilter";
    if (!criticalFilter.isNull()) sql += " AND critical = :criticalFilter";

    QSqlQuery query;
    query.prepare(sql);
    if (!start.isEmpty())        query.bindValue(":start", start);
    if (!end.isEmpty())          query.bindValue(":end", end);
    if (!configName.isEmpty())   query.bindValue(":configName", configName);
    if (!ackFilter.isNull()) {
        query.bindValue(":ackFilter", ackFilter.toBool() ? 1 : 0);
    }
    if (!criticalFilter.isNull()) {
        query.bindValue(":criticalFilter", criticalFilter.toBool() ? 1 : 0);
    }

    if (!query.exec()) {
        qWarning() << "[DATABASE] Range search failed:" << query.lastError().text();
        return results;
    }

    // Collect and return matching records
    while (query.next()) {
        QVariantMap rec;
        rec["id"]           = query.value("id");
        rec["config_name"]  = query.value("config_name");
        rec["old_value"]    = query.value("old_value").toString();
        rec["new_value"]    = query.value("new_value").toString();
        rec["acknowledged"] = query.value("acknowledged").toBool();
        rec["critical"]     = query.value("critical").toBool();
        rec["timestamp"]    = query.value("timestamp");
        results.append(rec);
    }
    return results;
}
