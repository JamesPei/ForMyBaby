#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <QString>
#include <QFileSystemModel>

class Utils{
public:
    Utils() = default;

    void getPhotos(std::vector<u_int32_t>& photos);

    void getPhotoByID(u_int32_t id);

    // bool uploadPhoto2Cloud(QString cloud="onedrive", QString file_path);

    bool upload(QString file_path, QString message, QString memory, QString datetime, QString position);

    bool combinePhoto(QString file_path, QString message, QString memory, QString datetime, QString position);

};

#endif // UTILS_H
