#include "utils.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QUrlQuery>
#include <QMimeDatabase>
#include <QDesktopServices>
#include <QTimer>
#include <QDebug>

Utils::Utils()
    : m_networkManager(new QNetworkAccessManager)
{
}

Utils::~Utils()
{
    delete m_networkManager;
}

void Utils::getPhotos(std::vector<u_int32_t>& photos)
{
}

void Utils::getPhotoByID(u_int32_t id)
{
}

bool Utils::upload(QString file_path, QString message, QString memory, QString datetime, QString position)
{
    upload2Ms(file_path);
    return false;
}

bool Utils::combinePhoto(QString file_path, QString message, QString memory, QString datetime, QString position)
{
    return true;
}

// ===============================================================
//  Microsoft OneDrive
// ===============================================================

bool Utils::isMsAuthenticated() const
{
    return !m_msAccessToken.isEmpty();
}

bool Utils::requestMsDeviceCode(QString& outUserCode, QString& outVerificationUrl, int& outInterval)
{
    QNetworkRequest request{QUrl(QString::fromLatin1(MS_DEVICE_CODE_URL))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery params;
    params.addQueryItem("client_id", QString::fromLatin1(MS_CLIENT_ID));
    params.addQueryItem("scope", QString::fromLatin1(MS_SCOPE));

    QNetworkReply* reply = m_networkManager->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Utils[MS]: requestDeviceCode failed:" << reply->errorString();
        reply->deleteLater();
        return false;
    }

    QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
    reply->deleteLater();

    outUserCode        = json["user_code"].toString();
    outVerificationUrl = json["verification_url"].toString();
    outInterval        = json["interval"].toInt(5);

    return !outUserCode.isEmpty();
}

bool Utils::pollMsToken(const QString& deviceCode, int interval)
{
    for (int i = 0; i < 300 / interval; ++i) {
        {
            QEventLoop waitLoop;
            QTimer::singleShot(interval * 1000, &waitLoop, &QEventLoop::quit);
            waitLoop.exec();
        }

        QNetworkRequest request{QUrl(QString::fromLatin1(MS_TOKEN_URL))};
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        QUrlQuery params;
        params.addQueryItem("client_id", QString::fromLatin1(MS_CLIENT_ID));
        params.addQueryItem("device_code", deviceCode);
        params.addQueryItem("grant_type", "urn:ietf:params:oauth:grant-type:device_code");

        QNetworkReply* reply = m_networkManager->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();

        if (json.contains("access_token")) {
            m_msAccessToken  = json["access_token"].toString();
            m_msRefreshToken = json["refresh_token"].toString();
            return true;
        }

        QString error = json["error"].toString();
        if (error == "authorization_pending") {
            continue;
        }
        if (error == "slow_down") {
            ++interval;
            continue;
        }
        qWarning() << "Utils[MS]: pollMsToken error:" << error;
        return false;
    }

    qWarning() << "Utils[MS]: pollMsToken timed out";
    return false;
}

bool Utils::refreshMsToken()
{
    if (m_msRefreshToken.isEmpty()) {
        return false;
    }

    QNetworkRequest request{QUrl(QString::fromLatin1(MS_TOKEN_URL))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery params;
    params.addQueryItem("client_id", QString::fromLatin1(MS_CLIENT_ID));
    params.addQueryItem("refresh_token", m_msRefreshToken);
    params.addQueryItem("grant_type", "refresh_token");

    QNetworkReply* reply = m_networkManager->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
    reply->deleteLater();

    if (json.contains("access_token")) {
        m_msAccessToken = json["access_token"].toString();
        if (json.contains("refresh_token")) {
            m_msRefreshToken = json["refresh_token"].toString();
        }
        return true;
    }

    qWarning() << "Utils[MS]: refreshMsToken failed";
    return false;
}

bool Utils::authenticateMs()
{
    QString userCode;
    QString verificationUrl;
    int interval;

    if (!requestMsDeviceCode(userCode, verificationUrl, interval)) {
        return false;
    }

    qInfo() << "============================================";
    qInfo() << "Utils[MS]: Open this URL to sign in:";
    qInfo() << "Utils[MS]:" << verificationUrl;
    qInfo() << "Utils[MS]: Enter this code:" << userCode;
    qInfo() << "============================================";

    QDesktopServices::openUrl(QUrl(verificationUrl));

    return pollMsToken(verificationUrl, interval);
}

bool Utils::uploadToOneDrive(const QString& filePath, const QString& accessToken)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Utils[MS]: Cannot open file:" << filePath;
        return false;
    }

    QByteArray fileData = file.readAll();
    file.close();

    QString fileName = QFileInfo(filePath).fileName();
    QString uploadUrl = QString::fromLatin1(MS_GRAPH_UPLOAD_URL).arg(fileName);

    QMimeDatabase mimeDb;
    QString mimeType = mimeDb.mimeTypeForFile(filePath).name();

    QNetworkRequest request{QUrl(uploadUrl)};
    request.setRawHeader("Authorization", ("Bearer " + accessToken).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);

    QNetworkReply* reply = m_networkManager->put(request, fileData);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    if (statusCode == 200 || statusCode == 201) {
        QJsonObject json = QJsonDocument::fromJson(responseData).object();
        QString webUrl = json["webUrl"].toString();
        qInfo() << "Utils[MS]: Upload successful:" << webUrl;
        return true;
    }

    qWarning() << "Utils[MS]: Upload failed, status:" << statusCode << responseData;
    return false;
}

QString Utils::upload2Ms(QString file_path)
{
    if (!QFileInfo::exists(file_path)) {
        qWarning() << "Utils[MS]: File not found:" << file_path;
        return "";
    }

    if (m_msAccessToken.isEmpty()) {
        if (!m_msRefreshToken.isEmpty()) {
            if (!refreshMsToken()) {
                if (!authenticateMs()) {
                    return "";
                }
            }
        } else {
            if (!authenticateMs()) {
                return "";
            }
        }
    }

    if (!uploadToOneDrive(file_path, m_msAccessToken)) {
        if (!refreshMsToken() || !uploadToOneDrive(file_path, m_msAccessToken)) {
            return "";
        }
    }

    return "onedrive://" + QFileInfo(file_path).fileName();
}

// ===============================================================
//  Google Drive
// ===============================================================

bool Utils::isGoogleAuthenticated() const
{
    return !m_googleAccessToken.isEmpty();
}

bool Utils::requestGoogleDeviceCode(QString& outUserCode, QString& outVerificationUrl, int& outInterval)
{
    QNetworkRequest request{QUrl(QString::fromLatin1(GOOGLE_DEVICE_CODE_URL))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery params;
    params.addQueryItem("client_id", QString::fromLatin1(GOOGLE_CLIENT_ID));
    params.addQueryItem("scope", QString::fromLatin1(GOOGLE_SCOPE));

    QNetworkReply* reply = m_networkManager->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Utils[Google]: requestDeviceCode failed:" << reply->errorString();
        reply->deleteLater();
        return false;
    }

    QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
    reply->deleteLater();

    outUserCode        = json["user_code"].toString();
    outVerificationUrl = json["verification_url"].toString();
    outInterval        = json["interval"].toInt(5);

    return !outUserCode.isEmpty();
}

bool Utils::pollGoogleToken(const QString& deviceCode, int interval)
{
    for (int i = 0; i < 300 / interval; ++i) {
        {
            QEventLoop waitLoop;
            QTimer::singleShot(interval * 1000, &waitLoop, &QEventLoop::quit);
            waitLoop.exec();
        }

        QNetworkRequest request{QUrl(QString::fromLatin1(GOOGLE_TOKEN_URL))};
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        QUrlQuery params;
        params.addQueryItem("client_id", QString::fromLatin1(GOOGLE_CLIENT_ID));
        params.addQueryItem("client_secret", QString::fromLatin1(GOOGLE_CLIENT_SECRET));
        params.addQueryItem("device_code", deviceCode);
        params.addQueryItem("grant_type", "urn:ietf:params:oauth:grant-type:device_code");

        QNetworkReply* reply = m_networkManager->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();

        if (json.contains("access_token")) {
            m_googleAccessToken  = json["access_token"].toString();
            m_googleRefreshToken = json["refresh_token"].toString();
            return true;
        }

        QString error = json["error"].toString();
        if (error == "authorization_pending") {
            continue;
        }
        if (error == "slow_down") {
            ++interval;
            continue;
        }
        qWarning() << "Utils[Google]: pollGoogleToken error:" << error;
        return false;
    }

    qWarning() << "Utils[Google]: pollGoogleToken timed out";
    return false;
}

bool Utils::refreshGoogleToken()
{
    if (m_googleRefreshToken.isEmpty()) {
        return false;
    }

    QNetworkRequest request{QUrl(QString::fromLatin1(GOOGLE_TOKEN_URL))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery params;
    params.addQueryItem("client_id", QString::fromLatin1(GOOGLE_CLIENT_ID));
    params.addQueryItem("client_secret", QString::fromLatin1(GOOGLE_CLIENT_SECRET));
    params.addQueryItem("refresh_token", m_googleRefreshToken);
    params.addQueryItem("grant_type", "refresh_token");

    QNetworkReply* reply = m_networkManager->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
    reply->deleteLater();

    if (json.contains("access_token")) {
        m_googleAccessToken = json["access_token"].toString();
        return true;
    }

    qWarning() << "Utils[Google]: refreshGoogleToken failed";
    return false;
}

bool Utils::authenticateGoogle()
{
    QString userCode;
    QString verificationUrl;
    int interval;

    if (!requestGoogleDeviceCode(userCode, verificationUrl, interval)) {
        return false;
    }

    qInfo() << "============================================";
    qInfo() << "Utils[Google]: Open this URL to sign in:";
    qInfo() << "Utils[Google]:" << verificationUrl;
    qInfo() << "Utils[Google]: Enter this code:" << userCode;
    qInfo() << "============================================";

    QDesktopServices::openUrl(QUrl(verificationUrl));

    return pollGoogleToken(verificationUrl, interval);
}

bool Utils::uploadToDriveAPI(const QString& filePath, const QString& accessToken, QString& outFileId)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Utils[Google]: Cannot open file:" << filePath;
        return false;
    }

    QByteArray fileData = file.readAll();
    file.close();

    QString fileName = QFileInfo(filePath).fileName();
    QMimeDatabase mimeDb;
    QString mimeType = mimeDb.mimeTypeForFile(filePath).name();

    QString boundary = "ForMyBaby_boundary_2024";
    QByteArray body;

    QJsonObject metadata;
    metadata["name"] = fileName;
    body.append("--" + boundary.toUtf8() + "\r\n");
    body.append("Content-Type: application/json; charset=UTF-8\r\n\r\n");
    body.append(QJsonDocument(metadata).toJson(QJsonDocument::Compact));
    body.append("\r\n");

    body.append("--" + boundary.toUtf8() + "\r\n");
    body.append("Content-Type: " + mimeType.toUtf8() + "\r\n\r\n");
    body.append(fileData);
    body.append("\r\n");
    body.append("--" + boundary.toUtf8() + "--\r\n");

    QNetworkRequest request{QUrl(QString::fromLatin1(GOOGLE_DRIVE_UPLOAD_URL))};
    request.setRawHeader("Authorization", ("Bearer " + accessToken).toUtf8());
    QString contentType = "multipart/related; boundary=" + boundary;
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);

    QNetworkReply* reply = m_networkManager->post(request, body);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    if (statusCode != 200) {
        qWarning() << "Utils[Google]: Upload failed, status:" << statusCode << responseData;
        return false;
    }

    QJsonObject json = QJsonDocument::fromJson(responseData).object();
    outFileId = json["id"].toString();
    return !outFileId.isEmpty();
}

QString Utils::upload2Google(QString file_path)
{
    if (!QFileInfo::exists(file_path)) {
        qWarning() << "Utils[Google]: File not found:" << file_path;
        return "";
    }

    if (m_googleAccessToken.isEmpty()) {
        if (!m_googleRefreshToken.isEmpty()) {
            if (!refreshGoogleToken()) {
                if (!authenticateGoogle()) {
                    return "";
                }
            }
        } else {
            if (!authenticateGoogle()) {
                return "";
            }
        }
    }

    QString fileId;
    if (!uploadToDriveAPI(file_path, m_googleAccessToken, fileId)) {
        if (!refreshGoogleToken() || !uploadToDriveAPI(file_path, m_googleAccessToken, fileId)) {
            return "";
        }
    }

    qInfo() << "Utils[Google]: Upload successful, file ID:" << fileId;
    return "https://drive.google.com/file/d/" + fileId + "/view";
}