#include "EncryptionUtils.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <openssl/evp.h>
#include <openssl/rand.h>

//------------------------------------------------------------------------------
// Static member definitions
//------------------------------------------------------------------------------

/** AES-256 encryption key (32 bytes) loaded from JSON file */
QByteArray EncryptionUtils::encryptionKey;

/** AES-CBC initialization vector (16 bytes) loaded from JSON file */
QByteArray EncryptionUtils::encryptionIv;

/** Timestamp of the last modification to the key file, for reload detection */
QDateTime EncryptionUtils::lastKeyFileModified;

/** QFileSystemWatcher to monitor key-file changes at runtime */
QFileSystemWatcher* EncryptionUtils::keyFileWatcher = nullptr;

//------------------------------------------------------------------------------
// Helper: Determine JSON key-file path
//------------------------------------------------------------------------------

/**
 * @brief Determine the absolute path to encryptionKeys.json.
 *
 * Uses a relative path under the app bundle’s resources directory,
 * with a special prefix on macOS vs. other platforms.
 *
 * @return Cleaned absolute file path as QString.
 */
QString EncryptionUtils::resolveEncryptionKeysPath()
{
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

//------------------------------------------------------------------------------
// Internal: Reload keys if the file has changed since last load
//------------------------------------------------------------------------------

/**
 * @brief Check file modification time and reload keys if updated.
 *
 * Compares the on-disk lastModified timestamp against lastKeyFileModified.
 * If newer, calls loadEncryptionKeys() to refresh key/IV in memory.
 */
void EncryptionUtils::maybeReloadKeys()
{
    QString filePath = resolveEncryptionKeysPath();
    QFileInfo fileInfo(filePath);

    // If we've never loaded keys or the file has been modified, reload.
    if (!lastKeyFileModified.isValid() ||
        fileInfo.lastModified() > lastKeyFileModified)
    {
        qDebug() << "[EncryptionUtils] Key file changed; reloading...";
        loadEncryptionKeys(filePath);
    }
}

//------------------------------------------------------------------------------
// Public: Load key & IV from JSON, base64-decoded
//------------------------------------------------------------------------------

/**
 * @brief Load the encryption key and IV from a JSON file.
 *
 * JSON format must contain base64-encoded strings under "key" and "iv".
 * Validates that key is 32 bytes and IV is 16 bytes before assignment.
 *
 * @param filePath Absolute path to the JSON file.
 */
void EncryptionUtils::loadEncryptionKeys(const QString &filePath)
{
    QFile file(filePath);
    if (!file.exists()) {
        qWarning() << "[EncryptionUtils] Key file not found:" << filePath;
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[EncryptionUtils] Cannot open key file:" << filePath;
        return;
    }

    // Parse JSON
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    QJsonObject obj = doc.object();

    // Validate fields
    if (!obj.contains("key") || !obj.contains("iv")) {
        qWarning() << "[EncryptionUtils] JSON missing 'key' or 'iv':" << filePath;
        return;
    }

    // Decode base64 values
    QByteArray keyStr = obj["key"].toString().toUtf8();
    QByteArray ivStr  = obj["iv"].toString().toUtf8();
    QByteArray newKey = QByteArray::fromBase64(keyStr);
    QByteArray newIv  = QByteArray::fromBase64(ivStr);

    // Validate lengths
    if (newKey.size() != 32) {
        qWarning() << "[EncryptionUtils] AES-256 key must be 32 bytes, got"
                   << newKey.size();
        return;
    }
    if (newIv.size() != 16) {
        qWarning() << "[EncryptionUtils] AES-CBC IV must be 16 bytes, got"
                   << newIv.size();
        return;
    }

    // Assign and record timestamp
    encryptionKey       = newKey;
    encryptionIv        = newIv;
    lastKeyFileModified = QFileInfo(filePath).lastModified();

    qDebug() << "[EncryptionUtils] Loaded key & IV from:" << filePath;
}

//------------------------------------------------------------------------------
// Public: Initialize once at startup
//------------------------------------------------------------------------------

/**
 * @brief One-time initialization:
 *  - Loads keys from disk.
 *  - Sets up QFileSystemWatcher to auto-reload on file changes.
 */
void EncryptionUtils::initialize()
{
    // Initial load
    QString path = resolveEncryptionKeysPath();
    loadEncryptionKeys(path);

    // Setup watcher if not already created
    if (!keyFileWatcher) {
        keyFileWatcher = new QFileSystemWatcher();
        keyFileWatcher->addPath(path);

        QObject::connect(keyFileWatcher,
                         &QFileSystemWatcher::fileChanged,
                         [=](const QString &changedPath) {
                             qDebug() << "[EncryptionUtils] Watcher detected change, reloading keys...";
                             loadEncryptionKeys(changedPath);
                             // Re-add path if watcher dropped it
                             if (!keyFileWatcher->files().contains(changedPath)) {
                                 keyFileWatcher->addPath(changedPath);
                             }
                         });
    }
}

//------------------------------------------------------------------------------
// Public: Encrypt plaintext to base64-encoded ciphertext
//------------------------------------------------------------------------------

/**
 * @brief AES-256-CBC encrypt a UTF-8 string, output as base64.
 *
 * Automatically reloads keys if the file has changed.
 *
 * @param data Plaintext QString to encrypt.
 * @return Base64-encoded ciphertext QByteArray, or empty on failure.
 */
QByteArray EncryptionUtils::encrypt(const QString &data)
{
    if (data.isEmpty()) {
        qWarning() << "[EncryptionUtils] Empty input; nothing to encrypt.";
        return {};
    }

    // Reload keys if needed
    maybeReloadKeys();

    // Ensure key/IV are loaded
    if (encryptionKey.isEmpty() || encryptionIv.isEmpty()) {
        qWarning() << "[EncryptionUtils] Key/IV not set; abort encrypt.";
        return {};
    }

    QByteArray input  = data.toUtf8();
    // Allocate output buffer: input + one block for padding
    QByteArray output(input.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()), 0);

    // Create OpenSSL context
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        qWarning() << "[EncryptionUtils] Failed to create EVP context.";
        return {};
    }

    // Initialize for AES-256-CBC
    if (!EVP_EncryptInit_ex(ctx,
                            EVP_aes_256_cbc(),
                            nullptr,
                            reinterpret_cast<const unsigned char*>(encryptionKey.data()),
                            reinterpret_cast<const unsigned char*>(encryptionIv.data())))
    {
        qWarning() << "[EncryptionUtils] EVP_EncryptInit_ex failed.";
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    int len = 0;
    int totalLen = 0;
    // EncryptUpdate
    if (!EVP_EncryptUpdate(ctx,
                           reinterpret_cast<unsigned char*>(output.data()),
                           &len,
                           reinterpret_cast<const unsigned char*>(input.data()),
                           input.size()))
    {
        qWarning() << "[EncryptionUtils] EVP_EncryptUpdate failed.";
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += len;

    // EncryptFinal (padding)
    if (!EVP_EncryptFinal_ex(ctx,
                             reinterpret_cast<unsigned char*>(output.data()) + totalLen,
                             &len))
    {
        qWarning() << "[EncryptionUtils] EVP_EncryptFinal_ex failed.";
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += len;
    output.resize(totalLen);

    EVP_CIPHER_CTX_free(ctx);

    // Return base64 ciphertext
    return output.toBase64();
}

//------------------------------------------------------------------------------
// Public: Decrypt base64-encoded ciphertext to plaintext QString
//------------------------------------------------------------------------------

/**
 * @brief AES-256-CBC decrypt base64-encoded data to QString.
 *
 * Automatically reloads keys if the file has changed.
 *
 * @param encryptedData Base64-encoded ciphertext QByteArray.
 * @return Decrypted UTF-8 QString, or empty on failure.
 */
QString EncryptionUtils::decrypt(const QByteArray &encryptedData)
{
    if (encryptedData.isEmpty()) {
        qWarning() << "[EncryptionUtils] Empty input; cannot decrypt.";
        return {};
    }

    // Reload keys if needed
    maybeReloadKeys();

    // Ensure key/IV are loaded
    if (encryptionKey.isEmpty() || encryptionIv.isEmpty()) {
        qWarning() << "[EncryptionUtils] Key/IV not set; abort decrypt.";
        return {};
    }

    // Decode from base64
    QByteArray cipher = QByteArray::fromBase64(encryptedData);
    QByteArray output(cipher.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()), 0);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        qWarning() << "[EncryptionUtils] Failed to create EVP context.";
        return {};
    }

    if (!EVP_DecryptInit_ex(ctx,
                            EVP_aes_256_cbc(),
                            nullptr,
                            reinterpret_cast<const unsigned char*>(encryptionKey.data()),
                            reinterpret_cast<const unsigned char*>(encryptionIv.data())))
    {
        qWarning() << "[EncryptionUtils] EVP_DecryptInit_ex failed.";
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    int len = 0;
    int totalLen = 0;
    // DecryptUpdate
    if (!EVP_DecryptUpdate(ctx,
                           reinterpret_cast<unsigned char*>(output.data()),
                           &len,
                           reinterpret_cast<const unsigned char*>(cipher.data()),
                           cipher.size()))
    {
        qWarning() << "[EncryptionUtils] EVP_DecryptUpdate failed.";
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += len;

    // DecryptFinal (verify padding)
    if (!EVP_DecryptFinal_ex(ctx,
                             reinterpret_cast<unsigned char*>(output.data()) + totalLen,
                             &len))
    {
        qWarning() << "[EncryptionUtils] EVP_DecryptFinal_ex failed: possible bad key/data.";
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += len;
    output.resize(totalLen);

    EVP_CIPHER_CTX_free(ctx);

    // Convert decrypted bytes to QString
    return QString::fromUtf8(output);
}
