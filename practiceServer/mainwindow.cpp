#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    bool server_started = false;

    server = new Server(server_started);

    if(server_started)
        {
            ui->infoAboutServerTextEdit->append("Сервер запущен");
        }
        else
        {
            ui->infoAboutServerTextEdit->append("Сервер не запущен");
        }

    connect(server, &Server::signalStatusServer, this, &MainWindow::slotStatusServer);

    nextBlockSize = 0;  //  обнуляем размер сообщения в самом начале работы
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::slotStatusServer(QString status)
{
    qDebug() << status;
    ui->infoAboutServerTextEdit->append(status);
}


void MainWindow::on_chooseSaveDirPushButton_clicked()
{
    QString dirPath = QFileDialog::getExistingDirectory(0, "Выбор папки", "");
    ui->infoAboutServerTextEdit->append("<font color = red>!!!<\\font> <br/> <font color = black><\\font>Установлена новая директория сохранения: "+dirPath+"<br/><font color = red>!!!<\\font>");
    ui->dirPathLabel->setText(dirPath);
}

