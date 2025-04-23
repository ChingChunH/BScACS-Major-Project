#ifndef ENCRYPTIONUTILS_H
#define ENCRYPTIONUTILS_H

#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QFileSystemWatcher>

/**
 * @brief EncryptionUtils provides static methods for encrypting and decrypting data
 * and managing encryption keys, including automatic reload on key file changes.
 */
class EncryptionUtils
{
public:
    /**
     * @brief Initializes the encryption subsystem.
     *        Loads keys and sets up a file watcher to reload keys if
     *        the key file changes on disk.
     */
    static void initialize();

    /**
     * @brief Encrypts plaintext data using the loaded encryption key and IV.
     * @param data The plaintext string to encrypt.
     * @return QByteArray containing the encrypted binary data.
     */
    static QByteArray encrypt(const QString &data);

    /**
     * @brief Decrypts previously encrypted data using the loaded key and IV.
     * @param encryptedData The binary data to decrypt.
     * @return QString containing the decrypted plaintext.
     */
    static QString decrypt(const QByteArray &encryptedData);

    /**
     * @brief Loads encryption keys (key and initialization vector) from the
     *        specified file path. Overrides any existing keys in memory.
     * @param filePath Filesystem path to the key file (e.g., JSON or binary format).
     */
    static void loadEncryptionKeys(const QString &filePath);

private:
    /**
     * @brief Resolves the default path to the encryption keys file.
     * @return QString containing the resolved file path.
     */
    static QString resolveEncryptionKeysPath();

    /**
     * @brief Checks if the key file has been modified since last load,
     *        and reloads keys if necessary.
     */
    static void maybeReloadKeys();

    /**
     * @brief Current encryption key (symmetric key) loaded from file.
     */
    static QByteArray encryptionKey;

    /**
     * @brief Current initialization vector (IV) for encryption operations.
     */
    static QByteArray encryptionIv;

    /**
     * @brief Timestamp of the last modification of the key file.
     */
    static QDateTime lastKeyFileModified;

    /**
     * @brief File system watcher monitoring the key file for changes.
     */
    static QFileSystemWatcher *keyFileWatcher;
};

#endif // ENCRYPTIONUTILS_H
