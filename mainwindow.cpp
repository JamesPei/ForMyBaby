#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "database.h"
#include <QProgressDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::ForMyBaby)
{
    ui->setupUi(this);

    // load record_tab
    QPixmap pixmap(":/static_resource/no-image.jpg");
    pixmap.scaled(960, 640, Qt::IgnoreAspectRatio);
    // 将图像设置为QLabel的背景
    ui->photo_window->setPixmap(pixmap);
    // 调整控件大小以适应图像大小
    ui->photo_window->resize(960, 640);

    ui->datetime->setDateTime(QDateTime::currentDateTime());

    // load memory_tab
    m_photoListModel = new QStringListModel(this);
    utils.getMemoryList(m_photoListModel);
    ui->photo_list->setModel(m_photoListModel);

    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openFile);
}

// 打开文件对话框，选择图片文件并显示在photo_window中
void MainWindow::openFile()
{
    // 弹出文件选择对话框，仅允许选择jpg/jpeg/png格式的图片
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("Images (*.jpg *jpeg *.png)"));
    if (!fileName.isEmpty())
    {
        curr_fileName = fileName;
        // 加载图片并缩放至 960x640，忽略原始宽高比
        QPixmap pixmap(fileName);
        QPixmap scaledPixmap = pixmap.scaled(960, 640, Qt::IgnoreAspectRatio);
        // 将缩放后的图片显示在 photo_window 控件中
        ui->photo_window->setPixmap(scaledPixmap);
    }
    else
    {
        // 用户未选择任何文件，弹出警告提示
        QMessageBox::warning(this, tr("Error"), tr("Not selelct any file"));
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
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

// Qt会自动扫描MainWindow中所有符合on_<对象名>_<信号名>()命名规则的槽函数，并自动将它们与对应控件的信号连接
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
    // 从UI控件中获取用户输入的数据
    QString message = ui->message->toPlainText();
    QString story = ui->story->toPlainText();
    QString position = ui->position->text();
    QString datetime = ui->datetime->dateTime().toString("yyyy-MM-dd HH:mm");
    QString imagePath = curr_fileName;

    qWarning() << "message:" << message;
    qWarning() << "story:" << story;
    qWarning() << "position:" << position;
    qWarning() << "date:" << datetime;
    qWarning() << "path:" << imagePath;


    // 显示不定进度条
    QProgressDialog progress(tr("正在上传…"), QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);   
    progress.setCancelButton(nullptr);             
    progress.setMinimumDuration(0);                
    progress.show();
    if (!utils.upload(imagePath, message, story, datetime, position))
    {
        qWarning() << "save record failed!";
    };
    progress.close();                              // 下载完成，关闭转轮

    // 将照片路径、消息、故事、时间、地点组合保存
    // utils.combinePhoto(imagePath, message, story, datetime, position);
}

void MainWindow::on_mainWidget_currentChanged(int index)
{
    if (index == 0)     // 0 = memory_tab（回忆页）
    { 
        utils.getMemoryList(m_photoListModel);
        ui->photo_list->setModel(m_photoListModel);
    }
}

void MainWindow::on_photo_list_clicked(const QModelIndex &index)
{
    QString record_id = index.data().toString();
    qInfo() << "Clicked:" << record_id;

    const auto &records = utils.getRecords();

    if (!records.contains(record_id)) {
        qWarning() << "record not found:" << record_id;
        return;
    }
    // value(key):返回对应值;使用operator[]有潜在风险，key不存在时会自动插入
    // 一个默认构造的值，然后返回它（会修改哈希表）
    BabyRecord record = records.value(record_id);
    QString cloud_photo_path = record.cloudPhotoPath;
    qInfo() << "cloud_photo_path:" << cloud_photo_path;

    // 在 photo_text 中显示记录的文字内容：先时间地点，再故事、留言
    ui->photo_text->setText(QStringLiteral("%1 · %2 故事: %3   留言: %4")
                                .arg(record.datetime, record.location,
                                     record.story, record.message));

    // 下载照片耗时较长，显示不定进度条（转轮）提示用户正在执行
    QProgressDialog progress(tr("正在加载照片…"), QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);   // 模态，防止下载期间重复点击
    progress.setCancelButton(nullptr);             // 禁止取消
    progress.setMinimumDuration(0);                // 立即显示（默认要等 4s）
    progress.show();

    QPixmap photo = utils.getPhotoByURL(cloud_photo_path);

    progress.close();                              // 下载完成，关闭转轮
    // 保持宽高比缩放图片以适配 photo_window2
    if (!photo.isNull()) {
        ui->photo_window2->setPixmap(photo.scaled(ui->photo_window2->size(),
                                                  Qt::KeepAspectRatio,
                                                  Qt::SmoothTransformation));
    }
}