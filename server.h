#ifndef SERVER_H
#define SERVER_H

#include "acceptor.h"

class Server
{
    boost::asio::io_service m_ios;
    std::unique_ptr<boost::asio::io_service::work> m_work;
    std::unique_ptr<Acceptor> m_acceptor;
    std::vector<std::unique_ptr<std::thread>> m_thread_pool;

    QSqlDatabase m_db;
    std::size_t m_count_of_threads;

private:
    void open_db(QSqlDatabase& db, const QString &name, const QString &host_name, const QString &user_name, const QString &password, int port);

public:
    Server();
    void start(unsigned short server_port_number, std::size_t thread_pool_size, const QString& db_name,
               const QString& db_host_name, const QString& db_user_name, const QString& db_password,
               int db_port_number);
    void join_threads();
};

#endif // SERVER_H
