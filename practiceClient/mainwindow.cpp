#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "components/frames/cardFrame/mainTabFrames/connectFrame/connect_frame.h" //  карточка подключения
#include "components/frames/cardFrame/mainTabFrames/possibleProcessingFrame/possible_processing_frame.h"  //  карточка обработок
#include "components/frames/cardFrame/mainTabFrames/chatFrame/chat_frame.h"   //  карточка чата
#include "components/frames/cardFrame/mainTabFrames/fileFrame/file_frame.h"   //  карточка файла
#include "components/frames/cardFrame/settingsTabFrames/selectWorkspaceFrame/select_workspace_frame.h"  //  карточка выбора рабочей папки
#include "components/frames/cardFrame/settingsTabFrames/selectProcessorFrame/select_processor_frame.h"  //  карточка выбора обработчика

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connectFrame = new ConnectFrame(this);

    possibleProcessingFrame = new PossibleProcessingFrame(this);
    chatFrame = new ChatFrame(this);
    fileFrame = new FileFrame(this);

    selectWorkspaceFrame = new SelectWorkspaceFrame(this);
    selectProcessorFrame = new SelectProcessorFrame();

    connectFrame->createInterface();
    possibleProcessingFrame->createInterface();
    chatFrame->createInterface();
    fileFrame->createInterface();

    selectWorkspaceFrame->createInterface();
    selectProcessorFrame->createInterface();

    mainContainer->addWidget(connectFrame);
    mainContainer->addWidget(possibleProcessingFrame);
    mainContainer->addWidget(chatFrame);
    mainContainer->addWidget(fileFrame);

    settingsContainer->addWidget(selectWorkspaceFrame);
    settingsContainer->addWidget(selectProcessorFrame);

    ui->mainFrame->setLayout(mainContainer);
    ui->settingsMainFrame->setLayout(settingsContainer);

    workspaceManager = new WorkspaceManager();
    connect(workspaceManager, &WorkspaceManager::signalStatusClient, this, &MainWindow::slotStatusClient);  //  связка для отображения статуса клиента, вывод в консоль

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setEnableInteface()
{
    client = new Client();

    QMap<QString, QVariant> mapValue = connectFrame->getValue();

    client->connectToHost(mapValue["IPLineEdit"].toString(), mapValue["portLineEdit"].toUInt());   //  подключение к серверу (локальный адрес + порт такой же, как у сервера)
    if(!(client->isOpen())){
        fileFrame->setValue("Проверьте IP и порт сервера! Вы не подключились");
        client = nullptr;
        return;
    }

    for(auto mainTabChild : ui->mainFrame->children()){
        if(QString(mainTabChild->metaObject()->className()).contains("Layout")){
            continue;
        }
        dynamic_cast<I_CardFrame*>(mainTabChild)->switchEnabledInteface();
    }

    connect(this, &MainWindow::signalSendTextToServer, client, &Client::slotSendTextToServer);
    connect(this, &MainWindow::signalSendFileToServer, client, &Client::slotSendFileToServer);
    connect(this, &MainWindow::signalSendToServer, client, &Client::slotSendToServer);
    connect(workspaceManager, &WorkspaceManager::signalSetClientFolders, client, &Client::slotSetClientFolders);
    connect(client, &Client::signalStatusClient, this, &MainWindow::slotStatusClient);
    connect(client, &Client::signalMessageTextBrowser, this, &MainWindow::slotMessageTextBrowser);
    connect(client, &Client::signalSetCBDataForm, this, &MainWindow::slotSetCBDataForm);
    connect(client, &Client::signalSetFilePathLabel, this, &MainWindow::slotSetFilePathLabel);
    connect(client, &Client::signalEnableInterface, this, &MainWindow::slotEnableInterface);


    chatFrame->setValue("Вы подключились!"+delimiter);
    possibleProcessingFrame->switchEnabledInteface();
}

void MainWindow::slotStatusClient(QString status)
{
    ui->consoleTextBrowser->append(QTime::currentTime().toString()+" | "+status+"</hr>");
}

void MainWindow::slotMessageTextBrowser(QString message)
{
    chatFrame->setValue(message);
}

void MainWindow::slotSetCBDataForm(QMap<QString, QVariant> possibleProcessingData)
{
    possibleProcessingFrame->setValue(possibleProcessingData);
}

void MainWindow::slotSetFilePathLabel(QString text)
{
    fileFrame->setValue(text);
}

void MainWindow::slotEnableInterface(QString message)
{
    for(auto mainTabChild : ui->mainFrame->children()){
        if(QString(mainTabChild->metaObject()->className()).contains("Layout")){
            continue;
        }
        dynamic_cast<I_CardFrame*>(mainTabChild)->switchEnabledInteface();
    }
    possibleProcessingFrame->switchEnabledInteface();
    chatFrame->setValue(message);
}

void MainWindow::on_chooseWorkspaceDirPushButton_clicked(){
    QString folderPath = QFileDialog::getExistingDirectory(0, "Выбор папки", "");  //  выбираем папку
    if(!folderPath.isEmpty()){
        //  для наглядности работы сохраняем путь в информационный QLabel
        //  при вызове setValue данный виджет сам вызовет сигнал для установки директории на сервере
        selectWorkspaceFrame->setValue(folderPath);
        ui->consoleTextBrowser->append(selectWorkspaceFrame->getValue().firstKey());

        qDebug() << "MainWindow::on_chooseWorkspaceDirPushButton_clicked:   " << folderPath;

        workspaceManager->setRootFolder(folderPath);
        client->setWorkspaceManager(workspaceManager);
        if(workspaceManager->createWorkspaceFolders()){
            ui->consoleTextBrowser->append("<hr/>Рабочая папка организована!");
            //  создаем наблюдатель за папкой Entry
            ui->consoleTextBrowser->append(workspaceManager->setEntryWatcher());
            ui->consoleTextBrowser->append(workspaceManager->setProcessedWatcher());

            //  включаем выбор обработок
            possibleProcessingFrame->switchEnabledInteface();
        } else {
            ui->consoleTextBrowser->append("<hr/>Рабочая папка не организована!");
        }
    }
}

void MainWindow::on_chooseProcessingPushButton_clicked()
{
    emit signalSendToServer("Set processing on client", dynamic_cast<PossibleProcessingFrame*>(possibleProcessingFrame)->getCurrentData());   //  отправляем серверу текущий текст в комбобоксе
    ui->consoleTextBrowser->append("Выбрано: "+dynamic_cast<PossibleProcessingFrame*>(possibleProcessingFrame)->getCurrentData()+delimiter);   //  пишем клиенту, что он выбрал
}
