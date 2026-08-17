#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVector>
#include <QDateTime>

struct BabyRecord {
    int id = -1;
    QString cloudPhotoPath;
    QString message;
    QString story;
    QString datetime;
    QString position;
};

class Database {
public:
    // 获取单例实例
    static Database& instance();

    // 初始化数据库连接，默认使用SQLite
    bool init(const QString& dbPath = "baby.db",
              const QString& driver = "QSQLITE");

    // 关闭数据库连接
    void close();

    // 创建表结构
    // bool createTables();

    // 插入一条记录，返回插入后的 id
    int insertRecord(const BabyRecord& record);

    // 更新记录
    bool updateRecord(const BabyRecord& record);

    // 删除记录
    bool deleteRecord(int id);

    // 查询单条记录
    BabyRecord getRecord(int id);

    // 获取所有记录
    QVector<BabyRecord> getAllRecords();

    // 根据关键字搜索（匹配 message 和 story 字段）
    QVector<BabyRecord> searchRecords(const QString& keyword);

    // 获取最近一次错误信息
    QString lastError() const;

    // 获取记录总数
    int recordCount();

private:
    Database() = default;
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // 将 QSqlQuery 结果集当前行转换为 BabyRecord
    BabyRecord rowToRecord(const QSqlQuery& query) const;

    QSqlDatabase m_db;
};

#endif // DATABASE_H