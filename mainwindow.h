#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QFileDialog>
#include <QResizeEvent>
#include "utils.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class ForMyBaby;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent * event);

private slots:
    void openFile();

    void on_clear_button_clicked();

    void on_save_button_clicked();

private:
    Ui::ForMyBaby *ui;
    QString curr_fileName;
    Utils utils;
};
#endif // MAINWINDOW_H
