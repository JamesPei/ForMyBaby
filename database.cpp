#include "database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

Database& Database::instance()
{
    static Database db;
    return db;
}

Database::~Database()
{
    close();
}

bool Database::init(const QString& dbPath, const QString& driver)
{
    // 使用 Qt SQL 驱动连接数据库，默认 QSQLITE
    m_db = QSqlDatabase::addDatabase(driver);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "Database open failed:" << m_db.lastError().text();
        return false;
    }

    // 创建表结构（如果不存在）
    QSqlQuery query(m_db);
    const QString sql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS baby_records ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "cloud_photo_path TEXT,"
        "message TEXT,"
        "story TEXT,"
        "datetime TEXT,"
        "position TEXT"
        ")"
    );

    if (!query.exec(sql)) {
        qWarning() << "Create table failed:" << query.lastError().text();
        return false;
    }

    return true;
}

void Database::close()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

int Database::insertRecord(const BabyRecord& record)
{
    QSqlQuery query(m_db);
    query.prepare(
        "INSERT INTO baby_records (cloud_photo_path, message, story, datetime, position) "
        "VALUES (:cloud_photo_path, :message, :story, :datetime, :position)"
    );
    query.bindValue(":cloud_photo_path", record.cloudPhotoPath);
    query.bindValue(":message",         record.message);
    query.bindValue(":story",           record.story);
    query.bindValue(":datetime",        record.datetime);
    query.bindValue(":position",        record.position);

    if (!query.exec()) {
        qWarning() << "Insert failed:" << query.lastError().text();
        return -1;
    }

    return query.lastInsertId().toInt();
}

bool Database::updateRecord(const BabyRecord& record)
{
    QSqlQuery query(m_db);
    query.prepare(
        "UPDATE baby_records SET "
        "cloud_photo_path = :cloud_photo_path, "
        "message = :message, "
        "story = :story, "
        "datetime = :datetime, "
        "position = :position "
        "WHERE id = :id"
    );
    query.bindValue(":cloud_photo_path", record.cloudPhotoPath);
    query.bindValue(":message",          record.message);
    query.bindValue(":story",            record.story);
    query.bindValue(":datetime",         record.datetime);
    query.bindValue(":position",         record.position);
    query.bindValue(":id",               record.id);

    if (!query.exec()) {
        qWarning() << "Update failed:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool Database::deleteRecord(int id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM baby_records WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "Delete failed:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

BabyRecord Database::getRecord(int id)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT * FROM baby_records WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec() || !query.next()) {
        qWarning() << "Get record failed:" << query.lastError().text();
        return BabyRecord();
    }

    return rowToRecord(query);
}

QVector<BabyRecord> Database::getAllRecords()
{
    QVector<BabyRecord> records;
    QSqlQuery query(m_db);

    if (!query.exec("SELECT * FROM baby_records ORDER BY id DESC")) {
        qWarning() << "Get all records failed:" << query.lastError().text();
        return records;
    }

    while (query.next()) {
        records.append(rowToRecord(query));
    }

    return records;
}

QVector<BabyRecord> Database::searchRecords(const QString& keyword)
{
    QVector<BabyRecord> records;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT * FROM baby_records "
        "WHERE message LIKE :keyword OR story LIKE :keyword "
        "ORDER BY id DESC"
    );
    query.bindValue(":keyword", "%" + keyword + "%");

    if (!query.exec()) {
        qWarning() << "Search failed:" << query.lastError().text();
        return records;
    }

    while (query.next()) {
        records.append(rowToRecord(query));
    }

    return records;
}

QString Database::lastError() const
{
    return m_db.lastError().text();
}

int Database::recordCount()
{
    QSqlQuery query(m_db);

    if (!query.exec("SELECT COUNT(*) FROM baby_records") || !query.next()) {
        qWarning() << "Count failed:" << query.lastError().text();
        return 0;
    }

    return query.value(0).toInt();
}

BabyRecord Database::rowToRecord(const QSqlQuery& query) const
{
    BabyRecord record;
    record.id             = query.value("id").toInt();
    record.cloudPhotoPath = query.value("cloud_photo_path").toString();
    record.message        = query.value("message").toString();
    record.story          = query.value("story").toString();
    record.datetime       = query.value("datetime").toString();
    record.position       = query.value("position").toString();
    return record;
}