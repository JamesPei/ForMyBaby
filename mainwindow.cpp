#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ForMyBaby)
{
    ui->setupUi(this);

    QPixmap pixmap(":/static_resource/no-image.jpg");
    pixmap.scaled(960, 640, Qt::IgnoreAspectRatio);
    // 将图像设置为QLabel的背景
    ui->photo_window->setPixmap(pixmap);
    // 调整控件大小以适应图像大小
    ui->photo_window->resize(960, 640);

    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openFile);
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("Images (*.jpg *jpeg *.png)"));
    if (!fileName.isEmpty())
    {
        curr_fileName = fileName;
        QPixmap pixmap(fileName);
        QPixmap scaledPixmap = pixmap.scaled(960, 640, Qt::IgnoreAspectRatio);
        ui->photo_window->setPixmap(scaledPixmap);
    }else{
        QMessageBox::warning(this, tr("Error"), tr("Not selelct any file"));
    }
}

void MainWindow::resizeEvent(QResizeEvent *event){
    // QSize newSize = event->size();
    // qInfo() << "new size:" << newSize.width() << " new height:" << newSize.height();

    // ui->centralwidget->setGeometry(0, 0, newSize.width(), newSize.height());

    // QRect v1(0, 0, newSize.width(), newSize.height());
    // ui->horizontalLayout_4->setGeometry(v1);

    // QRect v2(0, 0, newSize.width()*0.67, newSize.height()-10);
    // ui->verticalLayout_2->setGeometry(v2);
    // QRect v3(newSize.width()*0.67+10, 0, newSize.width(), newSize.height()-10);
    // ui->verticalLayout_3->setGeometry(v3);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_clear_button_clicked()
{
    ui->message->clear();
    ui->story->clear();
    ui->position->clear();
    QDate date(2023, 1, 1);
    QTime time(0, 0, 0);
    QDateTime photo_time(date, time);
    ui->datetime->setDateTime(photo_time);
}


void MainWindow::on_save_button_clicked()
{

}

