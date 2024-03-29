#include "expectation_manager.h"

ExpectationManager::ExpectationManager(QString rootFolder)
{
    this->rootFolder = rootFolder;
}

bool ExpectationManager::createFile(QString filePath)
{
    QFileInfo fileInfo(filePath);
    // установим текущую рабочую директорию, где будет файл, без QFileInfo может не заработать
    QDir::setCurrent(fileInfo.path());
    // Создаём объект файла и открываем его на запись
    QFile newFile(filePath);

    return newFile.copy(filePath, rootFolder+"/"+fileInfo.fileName());
}

QStringList ExpectationManager::getFiles()
{
    QDir currentFolder(rootFolder);
    QStringList files = {};
    QFileInfoList currentFileInfoList = currentFolder.entryInfoList(QDir::NoDotAndDotDot | QDir::Files);
    for(auto file : currentFileInfoList){
        files.append(file.absoluteFilePath());
    }
    return files;
}

bool ExpectationManager::removeFile(QString fileName)
{
    QFileInfo fileInfo(rootFolder+"/"+fileName);
    // установим текущую рабочую директорию, где будет файл, без QFileInfo может не заработать
    QDir::setCurrent(fileInfo.path());
    // Создаём объект файла
    QFile fileToDelete(rootFolder+"/"+fileName);

    return fileToDelete.remove();
}

bool ExpectationManager::setWatcher()
{
    //  при повторном вызове этой функции создается новый наблюдатель
    //  эта проверка позволит выйти из функции, минуя дублирования наблюдателей
    if(expectationFilesWatcher != nullptr){
        return true;
    }

    expectationFilesWatcher = new QFileSystemWatcher();

    if(expectationFilesWatcher->addPath(rootFolder)){
        connect(expectationFilesWatcher, &QFileSystemWatcher::directoryChanged, this, &ExpectationManager::slotExpectationDirectoryChanged);
        return true;
    }

    return false;
}

void ExpectationManager::slotExpectationDirectoryChanged(const QString &folderName)
{

}
