#include "server.h"

Server::Server() {
    m_work.reset(new boost::asio::io_service::work(m_ios));
}

void Server::open_db(QSqlDatabase& db, const QString &name, const QString &host_name, const QString &user_name, const QString &password, int port)
{
    QString connection_name = QString::number((quint64)QThread::currentThread(), 16);
    qDebug() << "connection name: " << connection_name << ", thread: " << QThread::currentThreadId();
    db = QSqlDatabase::addDatabase("QPSQL", connection_name);

    db.setDatabaseName(name);
    db.setHostName(host_name);
    db.setUserName(user_name);
    db.setPassword(password);
    db.setPort(port);

    if(db.open()) {
        qDebug() << "Database " << name << " was opened.";
    }
    else {
        qDebug() << "Database " << name << " was not opened.";
        qDebug() << db.lastError().text();
    }
}

void Server::start(unsigned short server_port_number, std::size_t thread_pool_size, const QString& db_name,
           const QString& db_host_name, const QString& db_user_name, const QString& db_password,
           int db_port_number)
{
    assert(thread_pool_size > 0);
    m_count_of_threads = thread_pool_size;
    m_acceptor.reset(new Acceptor(m_ios, server_port_number, m_db));
    m_acceptor->start();

    qDebug() << "Thread pool:";

    for(std::size_t i = 0; i < thread_pool_size; ++i) {
        std::unique_ptr<std::thread> th(new std::thread([this, i, db_name, db_host_name, db_user_name, db_password, db_port_number]()
        {
            qDebug() << "thread # " << i << " : " << QThread::currentThreadId();
            open_db(m_db, db_name, db_host_name, db_user_name, db_password, db_port_number);
            m_ios.run();
        }));
        m_thread_pool.push_back(std::move(th));
    }
}

void Server::join_threads()
{
    for(std::size_t i = 0; i <  m_count_of_threads; ++i) {
        m_thread_pool[i]->join();
    }
}
