#include "server.h"

Server::Server(bool &server_started){
    if(this->listen(QHostAddress::Any, 2323)){  //  статус будет передаваться, когда сервер будет прослушивать любой из адресов
        server_started = true;  //  меняем состояние сервера
        qDebug() << "start";    //  уведомляем в консоли
    } else {
        server_started = false; //  иначе что-то пошло не так
    }
    nextBlockSize = 0;  //  обнуляем размер сообщения в самом начале работы

    /// Глоссарий, описывающий тип отправляемого сообщения
    /// первое число определяет тип (0 - простой сигнал о чем-то \ 1 - запрос чего-то)
    /// второе число определяет конец какого-то действия, если оно в несколько этапов, например, передача файла
    /// третье и последующие числа определяют тип передаваемых данных

    mapRequest[""] = "";  //  ничего не нужно
    mapRequest["001"] = "Message";   //  отправляется простое сообщение
    mapRequest["0011"] = "Message from someone";    //  отправляется сообщение от кого-то конкретного
    mapRequest["002"] = "File";  //  отправляется файл (определяем начало процесса передачи файла)
    mapRequest["102"] = "Request part of file";  //  запрос на еще одну часть файла
    mapRequest["103"] = "Request part of processing file";  //  запрос на еще одну часть обрабатываемого файла
    mapRequest["012"] = "File downloaded";  //  файл загружен полностью (определяем конец процесса передачи файла)
    mapRequest["004"] = "Possible treatments ComboBox data";    //  отправка данных по доступным обработкам
    mapRequest["0041"] = "Set treatment on client";     //  закрепление возможной обработки за сокетом

    possibleTreatments[""] = "Отсутствие обработки";
    possibleTreatments["DOUBLE_INFO"] = "Дублирование информации";  //  содержимое файла дублируется в конец
    possibleTreatments["TRIPLE_INFO"] = "Утроение информации";  //  то же самое, но утраивается

    fileSystemWatcher = new QFileSystemWatcher;
    fileSystemWatcher->addPath(folderForRawInformation);    //  устанавливаем папку для слежки
    connect(fileSystemWatcher, SIGNAL(directoryChanged(QString)), this, SLOT(slotFolderForRawInformationChanged(QString)));

    /// TODO: сделать функцию, которая сама будет определять, какие типы обработок серверу нужны
    /// то есть она должна просматривать файл с настройками и добавлять их в выпадающий список

    // временный вариант с одной обработкой (пока не будет сохранения настроек):
    //
    //  НЕ РАБОТАЕТ
    //
    //Server::signalAddTreatmentToPossibleTreatmentsComboBox("DOUBLE_INFO");  //  программно добавляем в PossibleTreatmentsComboBox вид обработки
}

void Server::SendPossibleTreatments(QTcpSocket* socketForSend)
{
    Data.clear();   //  может быть мусор

    QDataStream out(&Data, QIODevice::WriteOnly);   //  объект out, режим работы только для записи, иначе ничего работать не будет
    out.setVersion(QDataStream::Qt_6_2);
    out << quint64(0) << mapRequest["004"] << possibleTreatments;  //  отправляем в поток размер_сообщения, тип-сообщения и строку при необходимости
    out.device()->seek(0);  //  в начало потока
    out << quint64(Data.size() - sizeof(quint64));  //  высчитываем размер сообщения
    socketForSend->write(Data);    //  отправляем по сокету данные
}

void Server::slotFolderForRawInformationChanged(const QString &folderName)
{
    QDir workWithDirectory;     //  создаем рабочую директорию
    workWithDirectory.cd(folderName); //  переходим в нужный каталог

    QStringList filters;    //  создаем список для фильтра
    filters << "DOUBLE_INFO*.txt";  //  заполняем

    workWithDirectory.setNameFilters(filters);  //  устанавливаем фильтр
    workWithDirectory.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);     //  устанавливаем фильтр выводимых файлов/папок
    workWithDirectory.setSorting(QDir::Size | QDir::Reversed);  //  устанавливаем сортировку "от меньшего к большему"
    QFileInfoList list = workWithDirectory.entryInfoList();     //  получаем список файлов директории
    for (int i = 0; i < list.size(); ++i) {
        QFileInfo fileInfo = list.at(i);
        qDebug() << qPrintable(QString("%1 %2").arg(fileInfo.size(), 10).arg(fileInfo.fileName()));   //  выводим в формате "размер имя"
        SendFileToClient(fileInfo.filePath());  //  отправляем файл клиенту
    }
    qDebug() << folderName;
    qDebug() << "================";     // переводим строку
}

void Server::slotSocketDisplayed(QTcpSocket* displayedSocket)
{
    SendPossibleTreatments(displayedSocket);
}

void Server::incomingConnection(qintptr socketDescriptor){  //  обработчик нового подключения
    socket = new QTcpSocket;    //  создание нового сокета под нового клиента
    socket->setSocketDescriptor(socketDescriptor);  //  устанавливаем в него дескриптор (- неотрицательное число, идентифицирующее  поток ввода-вывода)
    connect(socket, &QTcpSocket::readyRead, this, &Server::slotReadyRead);  //  связка готовности чтения
    connect(socket, &QTcpSocket::disconnected, this, &Server::slotDisconnect); //  связка удаления клиента

    mapSockets[socket] = "";    //  клиент пока не показал, что он умеет делать
    Server::signalStatusServer("new client on " + QString::number(socketDescriptor));   //  уведомление о подключении
    Server::signalAddSocketToListWidget(socket);    //  отображаем на форме в clientsListWidget этот сокет
    SendToAllClients(mapRequest["001"], "new client on " + QString::number(socketDescriptor)+delimiter);
    qDebug() << "new client on " << socketDescriptor;
    qDebug() << "push quantity of clients: "+QString::number(mapSockets.size());
}

void Server::slotReadyRead(){
    socket = (QTcpSocket*)sender(); //  записываем именно тот сокет, с которого пришел запрос
    QDataStream in(socket); //  создание объекта "in", помогающий работать с данными в сокете
    in.setVersion(QDataStream::Qt_6_2); //  установка версии, чтобы не было ошибок
    if(in.status() == QDataStream::Ok){ //  если у нас нет ошибок в состоянии работы in
        while(true){    //  цикл для расчета размера блока
            if(nextBlockSize == 0){ //  размер блока пока неизвестен
                qDebug() << "nextBlockSize == 0";
                qDebug() << "size of waiting bytes" << socket->bytesAvailable();   //  выводим размер ожидающих байтов
                if(socket->bytesAvailable() < 8){   //  и не должен быть меньше 8-и байт
                    qDebug() << "Data < 8, break";
                    break;  //  иначе выходим из цикла, т.е. размер посчитать невозможно
                }
                in >> nextBlockSize;    //  считываем размер блока в правильном исходе
            }
            if(socket->bytesAvailable() < nextBlockSize){   //  когда уже известен размер блока, мы сравниваем его с количеством байт, которые пришли от сервера
                qDebug() << "Data not full | socket->bytesAvailable() = "+QString::number(socket->bytesAvailable()) + " | nextBlockSize = "+QString::number(nextBlockSize);    //  если данные пришли не полностью
                break;
            }
            //  надо же, мы до сих пор в цикле, все хорошо

            QString typeOfMess;
            in >> typeOfMess;   //  считываем тип сообщения

            if(typeOfMess == "Message"){     //  если тип данных "Message"
                QString str;    //  создаем переменную строки
                in >> str;  //  записываем в нее строку из объекта in, чтобы проверить содержимое
                Server::signalStatusServer("User "+QString::number(socket->socketDescriptor())+" "+socket->localAddress().toString()+": "+str);     //  оформляем чат на стороне Сервера

                SendToAllClients(mapRequest["001"],"<font color = black><\\font>User "+QString::number(socket->socketDescriptor())+" "+socket->localAddress().toString()+": "+str.remove(0,5)+delimiter);      //  мы просто избавляемся от префикса "MESS:" и пересылаем клиенту сообщение
            }

            if(typeOfMess == "Message from someone"){   //  если сообщение с конкретным отправителем
                QString str, someone;    //  создаем переменную строки и отправителя
                in >> str >> someone;   //  считываем
                Server::signalStatusServer(someone+" "+QString::number(socket->socketDescriptor())+" "+socket->localAddress().toString()+": "+str);     //  оформляем чат на стороне Сервера
                SendToAllClients(mapRequest["001"],"<font color = black><\\font>"+someone+" "+QString::number(socket->socketDescriptor())+" "+socket->localAddress().toString()+": "+str.remove(0,5)+delimiter);      //  мы просто избавляемся от префикса "MESS:" и пересылаем клиенту сообщение
            }

            if(typeOfMess == "File"){    //  отправляется файл

                // mapRequest["002"] << fileName << fileSize

                if(fileName.isEmpty()){    //  если файла не существует
                    in >> fileName;  //  записываем из потока название файла
                    in >> fileSize; //  считываем его размер

                    if(fileSize < blockData){   //  если размер файла меньше выделенного блока
                        blockData = fileSize;   //  устанавливаем размер блока ровно по файлу (передача произойдет в один этап)
                    } else {
                        blockData = 1000000;  //  устанавливаем по умолчанию (на случай последующей передачи, если размер файла будет куда больше)
                    }

                    file = new QFile;     //  определяем файл
                    file->setFileName(fileName);    //  устанавливаем имя файла
                    QDir::setCurrent(newDirPath);  //  устанавливаем путь сохранения на рабочем столе
                    Server::signalStatusServer("Файл "+fileName+" создан на сервере");  //  уведомляем

                    SendToOneClient(socket, mapRequest["102"],"Downloading new part of file...");    //  запрашиваем первую часть файла
                }
            }

            if(typeOfMess == "Request part of processing file"){    //  от нас запрашивается новая часть файла
                QString str;    //  определяем переменную, в которую сохраним уведомление от запроса
                in >> str;  //  выводим в переменную сообщение

                qDebug() << str;  //  выводим в консоль
                Server::signalStatusServer(str); //  выводим клиенту

                nextBlockSize = 0;  //  заранее обнуляем размер сообщения
                SendPartOfFile();   //  вызываем соответствующий метод отправки
            }

            if(typeOfMess == "Request part of file"){   //  отправляется часть файла
                if((fileSize - file->size()) < blockData){  //  если разница между плановым и текущим размером файла меньше блока байтов
                    blockData = fileSize - file->size();    //  мы устанавливаем такой размер для блока (разницу)
                }

                bytes = new char[blockData];   //  выделяем байты под файл, то есть передача пройдет в несколько этапов

                in >> bytes;    //  считываем байты

                if(file->open(QIODevice::Append)){  //  записываем в конец файла
                    file->write(bytes, blockData);    //  записываем в файл
                } else {
                    Server::signalStatusServer("Не удается открыть файл "+fileName);
                }

                if(file->size() < fileSize){    //  если размер до сих пор не полон
                    Server::signalStatusServer("Текущий размер файла "+fileName+" от "+QString::number(socket->socketDescriptor())+" = "+QString::number(file->size())+"\n"+"Ожидаемый размер = "+QString::number(fileSize));

                    //  SendToAllClients(mapRequest["102"],"<font color = black><\\font>Downloading new part of file...<font color = black><\\font>");    //  запрашиваем новую часть файл
                    SendToOneClient(socket, mapRequest["102"],"<font color = black><\\font>Downloading new part of file...<font color = black><\\font>");    //  запрашиваем первую часть файла
                } else {
                    //  оформляем чат на стороне Сервера
                    //  уведомление о "кто: какой файл" при сигнале "012" - File downloaded
                    Server::signalStatusServer("User "+QString::number(socket->socketDescriptor())+" "+socket->localAddress().toString()+": send file by name \""+fileName+"\"");
                    //  SendToAllClients(mapRequest["012"],"<font color = green><\\font>User "+QString::number(socket->socketDescriptor())+" "+socket->localAddress().toString()+": send file by name \""+fileName+"\" \n"+delimiter);

                    SendToOneClient(socket, mapRequest["012"], "<font color = green><\\font>file \""+fileName+"\" downloaded \n"+delimiter);

                    //  TODO: при отправке всем происходит баг в задержке сообщений. решить
                    //  SendToAllClients(mapRequest["001"], "<font color = green><\\font>User "+QString::number(socket->socketDescriptor())+" "+socket->localAddress().toString()+": send file by name \""+fileName+"\" \n"+delimiter);

                    file->close();  //  закрываем файл
                    file = nullptr; //  удаляем файл
                    fileName.clear();   //  очищаем его название
                    fileSize = 0;   //  очищаем его размер
                    blockData = 1000000;  //  устанавливаем прежний размер байтов
                    delete[] bytes; //  удаляем байты из кучи
                    nextBlockSize = 0;  //  обнуляем для новых сообщений

                    return; //  выходим
                }

                file->close();   //закрываем файл
                if(bytes != nullptr){   //  удаляем байты из кучи, делая проверку на случай двойного удаления
                    delete[] bytes;
                    bytes = nullptr;
                }

            }   //  конец, если отправляется файл

            if(typeOfMess == "File downloaded"){ //  если файл полностью скачался
                QString str;    //  определяем переменную, в которую сохраним данные
                in >> str;  //  выводим в переменную сообщение
                qDebug() << "File "+fileName+" downloaded";   //  выводим консоль, какой файл был загружен
                Server::signalStatusServer(str);  //  и то же самое клиенту
                file->close();
                delete file; //  удаляем файл
                file = nullptr;
                delete[] bytes; //  удаляем байты из кучи
                nextBlockSize = 0;  //  обнуляем для новых сообщений
            }

            if(typeOfMess == "Set treatment on client"){
                QString currentTreatment;
                in >> currentTreatment;
                mapSockets[socket] = currentTreatment;
                qDebug() << mapSockets;
                if(currentTreatment != ""){
                    Server::signalStatusServer("<font color = blue><\\font>Client on "+QString::number(socket->socketDescriptor())+" "+socket->localAddress().toString()+" choose "+currentTreatment+" for processing"+delimiter);
                } else {
                    Server::signalStatusServer("<font color = red><\\font>Client on "+QString::number(socket->socketDescriptor())+" "+socket->localAddress().toString()+" removed the processing "+delimiter);
                }
            }

            nextBlockSize = 0;  //  обнуляем для новых сообщений
            if(socket->bytesAvailable() == 0){
                break;  //  выходим, делать больше нечего
            }
        }   //  конец while
    } else {
        Server::signalStatusServer("Something happened :(");    //  при ошибке чтения сообщения
    }
}

void Server::slotDisconnect()
{
    QTcpSocket* disconnectedSocket = static_cast<QTcpSocket*>(QObject::sender());
    mapSockets.remove(disconnectedSocket);
    qDebug() << "pop quantity of clients: "+QString::number(mapSockets.size());
    SendToAllClients(mapRequest["001"], "<font color = red><\\font>User  "+disconnectedSocket->localAddress().toString()+": has disconnected \n"+delimiter);
    Server::signalStatusServer("<font color = red><\\font>User  "+disconnectedSocket->localAddress().toString()+": has disconnected \n"+delimiter);
    Server::signalDeleteSocketFromListWidget(disconnectedSocket);
    disconnectedSocket->deleteLater();  //  оставляем удаление сокета программе
}

void Server::slotNewSaveDir(QString newDirPath) //  пока неработающий обработчик новой директории
{
    this->newDirPath = newDirPath;  //  установили новую директорию
}

void Server::SendToAllClients(QString typeOfMsg, QString str){ //  отправка клиенту сообщений
    Data.clear();   //  может быть мусор

    QDataStream out(&Data, QIODevice::WriteOnly);   //  объект out, режим работы только для записи, иначе ничего работать не будет
    out.setVersion(QDataStream::Qt_6_2);
    out << quint64(0) << typeOfMsg << str;  //  отправляем в поток размер_сообщения, тип-сообщения и строку при необходимости
    out.device()->seek(0);  //  в начало потока
    out << quint64(Data.size() - sizeof(quint64));  //  высчитываем размер сообщения

    auto it = mapSockets.begin();
    for(;it != mapSockets.end(); ++it)  //  пробегаемся по всем сокетам и
    {
        it.key()->write(Data);    //  отправляем по соответствующему сокету данные
    }
}

void Server::SendToOneClient(QTcpSocket* socket, QString typeOfMsg, QString str)
{
    Data.clear();   //  может быть мусор

    QDataStream out(&Data, QIODevice::WriteOnly);   //  объект out, режим работы только для записи, иначе ничего работать не будет
    out.setVersion(QDataStream::Qt_6_2);
    out << quint64(0) << typeOfMsg << str;  //  отправляем в поток размер_сообщения, тип-сообщения и строку при необходимости
    out.device()->seek(0);  //  в начало потока
    out << quint64(Data.size() - sizeof(quint64));  //  высчитываем размер сообщения
    socket->write(Data);    //  отправляем по сокету данные
}

void Server::SendFileToClient(QString filePath)
{
    Data.clear();   //  чистим массив байт от мусора
    file = new QFile(filePath);   //  определяем файл, чтобы поработать с его свойствами и данными
    fileSize = file->size();     //  определяем размер файла
    QFileInfo fileInfo(file->fileName());    //  без этой строки название файла будет хранить полный путь до него
    fileName = fileInfo.fileName();     //  записываем название файла

    Server::signalStatusServer("Size: "+QString::number(fileSize)+" Name: "+fileName);  //  простое уведомление пользователя о размере и имени файла, если мы смогли его найти

    if(fileSize < blockData){   //  если размер файла меньше выделенного блока
        blockData = fileSize;
    } else {    //  если мы еще раз отправляем какой-нибудь файл
        blockData = 1000000;  //  возвращаем дефолтное значение
    }
    bytes = new char[blockData];   //  выделяем байты под файл, то есть передача пройдет в несколько этапов

    for(auto iteratorSocket = mapSockets.begin(); iteratorSocket != mapSockets.end(); iteratorSocket++){
        //  проверяем только начало с префиксом у сокета, потому что в эту функцию попадают файлы по фильтру
        if(fileName.startsWith(iteratorSocket.value())){
            socket = iteratorSocket.key();  //  присуждаем переменной ссылку на сокет

            if(file->open(QIODevice::ReadOnly)){ //  открываем файл для только чтения
                socket->waitForBytesWritten();  //  мы ждем того, чтобы все байты записались

                QDataStream out(&Data, QIODevice::WriteOnly);   //  определяем поток отправки
                out.setVersion(QDataStream::Qt_6_2);
                out << quint64(0) << mapRequest["002"] << fileName << fileSize;   //  отправляем название файла и его размер
                out.device()->seek(0);
                //  избавляемся от зарезервированных двух байт в начале каждого сообщения
                out << quint64(Data.size() - sizeof(quint64));   //  определяем размер сообщения
                socket->write(Data);
            } else {
                Server::signalStatusServer("File"+fileName+" not open :(");
            }
        }
    }
}

void Server::SendPartOfFile()
{
    if((fileSize - file->pos()) < blockData){   //  если остаток файла будет меньше блока байт
        blockData = fileSize - file->pos();     //  мы просто будем читать этот остаток
    }

    socket->waitForBytesWritten();  //  мы ждем того, чтобы все байты записались
    Data.clear();

    qDebug() << "read " << file->read(bytes, blockData);     //  читаем файл и записываем данные в байты
    qDebug() << "block: "+QString::number(blockData);   //  нужно, чтобы видеть текущий размер блоков


    QByteArray buffer;
    buffer = buffer.fromRawData(bytes, blockData);

    qDebug() << "block size" << blockData << "buffer size" << buffer.size();

    QDataStream out(&Data, QIODevice::WriteOnly);   //  определяем поток отправки
    out.setVersion(QDataStream::Qt_6_2);
    out << quint64(0) << mapRequest["103"] << buffer;   //  отправляем байты
    out.device()->seek(0);
    //  избавляемся от зарезервированных двух байт в начале каждого сообщения
    qDebug() << "sending blockSize = " << quint64(Data.size() - sizeof(quint64));
    out << quint64(Data.size() - sizeof(quint64));   //  определяем размер сообщения
    socket->write(Data);
    qDebug() << "Data size = " << Data.size();
}

