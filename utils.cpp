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
#include <QMessageBox>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

Utils::Utils()
    : m_networkManager(new QNetworkAccessManager)
{
    loadMsTokens();
    loadGoogleTokens();
    if (!Database::instance().init()) {
        qWarning() << "Database Init failed!";
    }
}

Utils::~Utils()
{
    delete m_networkManager;
}

QHash<QString, BabyRecord>& Utils::getRecords(){
    return records;
};

void Utils::getMemoryList(QStringListModel* listModel){
    QStringList storyList;
    for (const auto &rec : Database::instance().getAllRecords())
    {
        QString record_id = QString::number(rec.id) + "_" + rec.datetime;
        qInfo() << record_id;

        // 仅当 records 中不存在该 record_id 时才插入，避免重复覆盖
        if (!records.contains(record_id))
        {
            BabyRecord temp_record;
            temp_record.cloudPhotoPath = rec.cloudPhotoPath;
            temp_record.story = rec.story;
            temp_record.message = rec.message;
            temp_record.datetime = rec.datetime;
            temp_record.location = rec.location;

            records.insert(record_id, temp_record);
        }
        storyList << record_id;
    }

    listModel->setStringList(storyList);
};

void Utils::getPhotos(std::vector<u_int32_t>& photos)
{
}

void Utils::getPhotoByID(u_int32_t id)
{
}

QPixmap Utils::getPhotoByURL(QString url){
    // if(url.startsWith("https://onedrive.live.com")){
        return getPhotoFromMs(url);
    // }
}

bool Utils::upload(QString file_path, QString message, QString memory, QString datetime, QString location)
{
    QString onedrive_url = upload2Ms(file_path);
    qInfo() << "upload to: " << onedrive_url;

    if (onedrive_url.isEmpty()) {
        return false;
    }

    BabyRecord record;
    record.cloudPhotoPath = onedrive_url;
    record.message = message;
    record.story = memory;
    record.datetime = datetime;
    record.location = location;

    Database::instance().insertRecord(record);

    return true;
}

bool Utils::combinePhoto(QString file_path, QString message, QString memory, QString datetime, QString location)
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

// 向 Microsoft 设备码端点发起请求，获取用户验证码和验证URL
// outUserCode: 用户需要在浏览器中输入的验证码
// outDeviceCode: 用于轮询 token 的设备码
// outVerificationUrl: 用户需要打开的验证页面地址
// outInterval: 轮询间隔（秒），默认5秒
bool Utils::requestMsDeviceCode(QString& outUserCode, QString& outDeviceCode, QString& outVerificationUrl, int& outInterval)
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

    // Microsoft 的字段名为 verification_uri（不是 verification_url）
    QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
    reply->deleteLater();

    outUserCode        = json["user_code"].toString();
    outDeviceCode      = json["device_code"].toString();
    outVerificationUrl = json["verification_uri"].toString();
    outInterval        = json["interval"].toInt(5);

    return !outUserCode.isEmpty();
}

// 轮询Microsoft token端点，等待用户在浏览器中完成授权
// 最长等待300秒，超过则超时退出
bool Utils::pollMsToken(const QString& deviceCode, int interval)
{
    for (int i = 0; i < 300 / interval; ++i) {
        // 等待指定间隔后再发起下一次请求
        {
            QEventLoop waitLoop;
            QTimer::singleShot(interval * 1000, &waitLoop, &QEventLoop::quit);
            waitLoop.exec();
        }

        // 用 device_code 向 token 端点发起 POST 请求
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

        // 用户已完成授权，保存 token
        if (json.contains("access_token")) {
            m_msAccessToken  = json["access_token"].toString();
            m_msRefreshToken = json["refresh_token"].toString();
            saveMsTokens();
            return true;
        }

        // authorization_pending: 用户尚未完成授权，继续等待
        // slow_down: 服务器要求降低轮询频率
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
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    if (json.contains("access_token")) {
        m_msAccessToken = json["access_token"].toString();
        if (json.contains("refresh_token")) {
            m_msRefreshToken = json["refresh_token"].toString();
        }
        saveMsTokens();
        return true;
    }

    qWarning() << "Utils[MS]: refreshMsToken failed, status:" << statusCode
               << "error:" << json["error"].toString()
               << "description:" << json["error_description"].toString();

    // 清空已失效的 token，下次操作会触发重新认证
    m_msAccessToken.clear();
    m_msRefreshToken.clear();
    saveMsTokens();
    return false;
}

bool Utils::authenticateMs()
{
    QString userCode;
    QString deviceCode;
    QString verificationUrl;
    int interval;

    if (!requestMsDeviceCode(userCode, deviceCode, verificationUrl, interval)) {
        return false;
    }

    qInfo() << "============================================";
    qInfo() << "Utils[MS]: Open this URL to sign in:";
    qInfo() << "Utils[MS]:" << verificationUrl;
    qInfo() << "Utils[MS]: Enter this code:" << userCode;
    qInfo() << "============================================";

    QDesktopServices::openUrl(QUrl(verificationUrl));

    if (!userCode.isEmpty()) {
        QMessageBox::information(nullptr, "Microsoft验证码",
            "在浏览器打开的页面中输入以下验证码：\n\n" + userCode);
    }

    return pollMsToken(deviceCode, interval);
}

bool Utils::uploadToOneDrive(const QString& filePath, const QString& accessToken, QString& cloud_url)
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
        QMessageBox::information(nullptr, "", "上传成功");
        cloud_url = webUrl;
        return true;
    }else{
        qWarning() << "Utils[MS]: Upload failed, status:" << statusCode << responseData;
        QMessageBox::information(nullptr, "", "上传失败");
        cloud_url = "";
        return false;
    }
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

    QString cloud_url;
    if (!uploadToOneDrive(file_path, m_msAccessToken, cloud_url)) {
        if (!refreshMsToken() || !uploadToOneDrive(file_path, m_msAccessToken, cloud_url)) {
            return "";
        }
    }

    return cloud_url;
}

QPixmap Utils::getPhotoFromMs(QString url)
{
    // 从 OneDrive 分享链接中提取文件 ID
    QUrl shareUrl(url);
    QUrlQuery query(shareUrl);
    QString itemId = query.queryItemValue("id");

    if (itemId.isEmpty()) {
        qWarning() << "Utils[MS]: Cannot extract item ID from URL:" << url;
        return QPixmap();
    }

    // 确保有有效的 access token
    qInfo() << "Utils[MS]: Download - accessToken empty?" << m_msAccessToken.isEmpty()
            << "refreshToken empty?" << m_msRefreshToken.isEmpty();

    if (m_msAccessToken.isEmpty()) {
        if (!m_msRefreshToken.isEmpty()) {
            qInfo() << "Utils[MS]: Download - trying refreshMsToken";
            bool ok = refreshMsToken();
            qInfo() << "Utils[MS]: Download - refreshMsToken result:" << ok
                    << "accessToken now empty?" << m_msAccessToken.isEmpty();
        }
        if (m_msAccessToken.isEmpty()) {
            qWarning() << "Utils[MS]: Not authenticated";
            return QPixmap();
        }
    }

    qInfo() << "Utils[MS]: Download - accessToken length:" << m_msAccessToken.length();

    // 通过 Microsoft Graph API 下载文件内容
    QString downloadUrl = QString("https://graph.microsoft.com/v1.0/me/drive/items/%1/content").arg(itemId);

    QNetworkRequest request{QUrl(downloadUrl)};
    request.setRawHeader("Authorization", ("Bearer " + m_msAccessToken).toUtf8());
    // 不自动跟随重定向：Graph API 的 /content 端点返回 302，
    // 自动跟随会丢失 Authorization 头导致 401
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);

    QNetworkReply* reply = m_networkManager->get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray data = reply->readAll();

    // 处理 302 重定向：Graph API 的 /content 端点返回重定向到预签名下载 URL
    if (statusCode == 302) {
        QString redirectUrl = reply->rawHeader("Location");
        reply->deleteLater();
        if (!redirectUrl.isEmpty()) {
            qInfo() << "Utils[MS]: Following redirect to download";
            QNetworkRequest redirectRequest{QUrl(redirectUrl)};
            // 预签名 URL 不需要 Authorization 头
            QNetworkReply* redirectReply = m_networkManager->get(redirectRequest);
            QEventLoop redirectLoop;
            QObject::connect(redirectReply, &QNetworkReply::finished, &redirectLoop, &QEventLoop::quit);
            redirectLoop.exec();

            statusCode = redirectReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            data = redirectReply->readAll();
            redirectReply->deleteLater();
        }
    } else {
        reply->deleteLater();
    }

    if (statusCode == 401) {
        qInfo() << "Utils[MS]: Download - got 401, calling refreshMsToken";
        if (refreshMsToken()) {
            qInfo() << "Utils[MS]: Download - refreshMsToken succeeded, retrying";
            QNetworkRequest retryRequest{QUrl(downloadUrl)};
            retryRequest.setRawHeader("Authorization", ("Bearer " + m_msAccessToken).toUtf8());
            retryRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                     QNetworkRequest::ManualRedirectPolicy);

            QNetworkReply* retryReply = m_networkManager->get(retryRequest);
            QEventLoop retryLoop;
            QObject::connect(retryReply, &QNetworkReply::finished, &retryLoop, &QEventLoop::quit);
            retryLoop.exec();

            statusCode = retryReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            data = retryReply->readAll();

            if (statusCode == 302) {
                QString redirectUrl = retryReply->rawHeader("Location");
                retryReply->deleteLater();
                if (!redirectUrl.isEmpty()) {
                    QNetworkRequest redirectRequest{QUrl(redirectUrl)};
                    QNetworkReply* redirectReply = m_networkManager->get(redirectRequest);
                    QEventLoop redirectLoop2;
                    QObject::connect(redirectReply, &QNetworkReply::finished, &redirectLoop2, &QEventLoop::quit);
                    redirectLoop2.exec();

                    statusCode = redirectReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    data = redirectReply->readAll();
                    redirectReply->deleteLater();
                }
            } else {
                retryReply->deleteLater();
            }
            qInfo() << "Utils[MS]: Download - retry status:" << statusCode;
        } else {
            qInfo() << "Utils[MS]: Download - refreshMsToken failed";
        }
    }

    if (statusCode != 200) {
        qWarning() << "Utils[MS]: Download failed, url:" << downloadUrl
                   << "status:" << statusCode
                   << "response:" << data;
        return QPixmap();
    }

    QPixmap pixmap;
    pixmap.loadFromData(data);
    return pixmap;
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
            saveGoogleTokens();
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
        saveGoogleTokens();
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

// ===============================================================
//  Token 持久化
// ===============================================================

static QString msTokenPath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/ms_tokens.json";
}

static QString googleTokenPath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/google_tokens.json";
}

void Utils::loadMsTokens()
{
    QFile file(msTokenPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    m_msAccessToken  = json["access_token"].toString();
    m_msRefreshToken = json["refresh_token"].toString();
}

void Utils::saveMsTokens()
{
    QJsonObject json;
    json["access_token"]  = m_msAccessToken;
    json["refresh_token"] = m_msRefreshToken;

    QFile file(msTokenPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(json).toJson());
        file.close();
    }
}

void Utils::loadGoogleTokens()
{
    QFile file(googleTokenPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    m_googleAccessToken  = json["access_token"].toString();
    m_googleRefreshToken = json["refresh_token"].toString();
}

void Utils::saveGoogleTokens()
{
    QJsonObject json;
    json["access_token"]  = m_googleAccessToken;
    json["refresh_token"] = m_googleRefreshToken;

    QFile file(googleTokenPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(json).toJson());
        file.close();
    }
}