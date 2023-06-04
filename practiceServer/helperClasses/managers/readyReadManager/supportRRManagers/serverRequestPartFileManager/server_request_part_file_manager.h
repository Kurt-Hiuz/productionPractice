#ifndef CLIENTSREQUESTPARTPROCESSINGFILE_H
#define CLIENTSREQUESTPARTPROCESSINGFILE_H

///     Класс ClientsRequestPartFileManager реализует интерфейс I_MessageManager
///     обрабатывает всё необходимое, что связанно с запросами на части файлов
///     Переменные:
///         str - сообщение в консоль
///     Методы:
///         readDataFromStream() - чтение данных с потока
///         writeDataFromStream() - запись данных в поток
///         processData() - обрабатывает приходящие данные
///         typeOfMessage() - возвращает строку тип менеджера
///     Сигналы:
///         signalStatusRRManager() - передача ReadyReadManager статуса сервера
///         signalSendToOneRRManager() - отправка данных конкретному сокету через ReadyReadManager

///  ========================    классы проекта
#include "../I_message_manager.h"   //  реализуем интерфейс
///  ========================
///
///  ========================    для работы с файлами
#include <QFile>
///  ========================

class ServerRequestPartFileManager : public I_MessageManager
{
    Q_OBJECT
public:
    ServerRequestPartFileManager();
    void readDataFromStream(QDataStream &inStream) override;
    void writeDataToStream(QDataStream &outStream) override;
    void processData(QDataStream &inStream, QTcpSocket *socket) override;
    QString typeOfMessage() override;

    void setFilePath(QString &filePath);

signals:
    void signalStatusRRManager(QString status);
    void signalSendBufferRRManager(QByteArray &buffer);

private:
    QString str;
    int fileSize;   //  размер файла
    QString fileName;   //  его название
    QFile *file;     //  сам файлик
    char *bytes = nullptr;     //  массив байт данных
    int blockData = 1000000;  //  размер данных
    QByteArray buffer;

//  переопределение операторов >> и <<
protected:
    friend QDataStream &operator >> (QDataStream &in, ServerRequestPartFileManager &serverRequestPartFileManager);
    friend QDataStream &operator << (QDataStream &out, ServerRequestPartFileManager &serverRequestPartFileManager);

public slots:
    void slotClearFileData();
};

#endif // CLIENTSREQUESTPARTPROCESSINGFILE_H