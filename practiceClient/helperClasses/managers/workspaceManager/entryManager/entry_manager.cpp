#include "entry_manager.h"

EntryManager::EntryManager(QString rootFolder)
{
    this->rootFolder = rootFolder;
}

bool EntryManager::setWatcher()
{
    //  при повторном вызове этой функции создается новый наблюдатель
    //  эта проверка позволит выйти из функции, минуя дублирования наблюдателей
    if(entryFilesWatcher != nullptr){
        return true;
    }

    entryFilesWatcher = new QFileSystemWatcher();

    if(entryFilesWatcher->addPath(rootFolder)){
        connect(entryFilesWatcher, &QFileSystemWatcher::directoryChanged, this, &EntryManager::slotEntryDirectoryChanged);
        return true;
    }

    return false;
}

bool EntryManager::removeFile(QString filePath)
{
    QFileInfo fileInfo(filePath);
    // установим текущую рабочую директорию, где будет файл, без QFileInfo может не заработать
    QDir::setCurrent(fileInfo.path());
    // Создаём объект файла и открываем его на запись
    QFile fileToDelete(filePath);

    return fileToDelete.remove();
}

void EntryManager::slotEntryDirectoryChanged(const QString &folderName)
{
    processingManager = new ProcessingManager();
    QFileInfoList list = processingManager->entryFiles(folderName);     //  получаем список файлов директории

    for (int i = 0; i < list.size(); i++) {
        QFileInfo fileInfo = list.at(i);
        emit signalNewEntryFile(fileInfo);
    }
    if(processingManager != nullptr){
        delete processingManager;
        processingManager = nullptr;
    }
}
