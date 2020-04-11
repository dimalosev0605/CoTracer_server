#ifndef ACCEPTOR_H
#define ACCEPTOR_H

#include <QSqlDatabase>

#include "service.h"

class Acceptor
{
    boost::asio::io_service& m_ios;
    boost::asio::ip::tcp::acceptor m_acceptor;
    const QSqlDatabase& db;

private:
    void init_accept();
    void on_accept(const boost::system::error_code& ec, std::shared_ptr<boost::asio::ip::tcp::socket> sock);

public:
    Acceptor(boost::asio::io_service& ios, unsigned short port_number, const QSqlDatabase& db);
    void start();
};

#endif // ACCEPTOR_H
