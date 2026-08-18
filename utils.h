#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <QString>
#include <QFileSystemModel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "database.h"

class Utils {
public:
    Utils();
    ~Utils();

    void getPhotos(std::vector<u_int32_t>& photos);

    void getPhotoByID(u_int32_t id);

    QPixmap getPhotoByURL(QString url);

    bool upload(QString file_path, QString message, QString memory, QString datetime, QString location);

    bool combinePhoto(QString file_path, QString message, QString memory, QString datetime, QString location);

    // ─── OneDrive (Microsoft Graph API) ────────────────────────────
    QString upload2Ms(QString file_path);
    bool authenticateMs();
    bool isMsAuthenticated() const;
    QPixmap getPhotoFromMs(QString url);

    // ─── Google Drive ──────────────────────────────────────────────
    QString upload2Google(QString file_path);
    bool authenticateGoogle();
    bool isGoogleAuthenticated() const;

private:
    // ─── Microsoft OAuth ───────────────────────────────────────────
    bool requestMsDeviceCode(QString& outUserCode, QString& outDeviceCode, QString& outVerificationUrl, int& outInterval);
    bool pollMsToken(const QString& deviceCode, int interval);
    bool refreshMsToken();
    bool uploadToOneDrive(const QString& filePath, const QString& accessToken, QString& cloud_url);
    void loadMsTokens();
    void saveMsTokens();

    // ─── Google OAuth ──────────────────────────────────────────────
    bool requestGoogleDeviceCode(QString& outUserCode, QString& outVerificationUrl, int& outInterval);
    bool pollGoogleToken(const QString& deviceCode, int interval);
    bool refreshGoogleToken();
    bool uploadToDriveAPI(const QString& filePath, const QString& accessToken, QString& outFileId);
    void loadGoogleTokens();
    void saveGoogleTokens();

    // ─── Microsoft 凭据 ────────────────────────────────────────────
    static constexpr const char* MS_CLIENT_ID = "4d3c6347-8476-4090-a42e-79bb923ddcba";

    static constexpr const char* MS_DEVICE_CODE_URL =
        "https://login.microsoftonline.com/common/oauth2/v2.0/devicecode";
    static constexpr const char* MS_TOKEN_URL =
        "https://login.microsoftonline.com/common/oauth2/v2.0/token";
    static constexpr const char* MS_GRAPH_UPLOAD_URL =
        "https://graph.microsoft.com/v1.0/me/drive/root:/ForMyBaby/%1:/content";

    static constexpr const char* MS_SCOPE = "Files.ReadWrite offline_access";

    // ─── Google 凭据 ───────────────────────────────────────────────
    static constexpr const char* GOOGLE_CLIENT_ID     = "YOUR_GOOGLE_CLIENT_ID";
    static constexpr const char* GOOGLE_CLIENT_SECRET = "YOUR_GOOGLE_CLIENT_SECRET";

    static constexpr const char* GOOGLE_DEVICE_CODE_URL =
        "https://oauth2.googleapis.com/device/code";
    static constexpr const char* GOOGLE_TOKEN_URL =
        "https://oauth2.googleapis.com/token";
    static constexpr const char* GOOGLE_DRIVE_UPLOAD_URL =
        "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart";

    static constexpr const char* GOOGLE_SCOPE = "https://www.googleapis.com/auth/drive.file";

    QNetworkAccessManager* m_networkManager;

    // Microsoft tokens
    QString m_msAccessToken;
    QString m_msRefreshToken;

    // Google tokens
    QString m_googleAccessToken;
    QString m_googleRefreshToken;
};

#endif // UTILS_H