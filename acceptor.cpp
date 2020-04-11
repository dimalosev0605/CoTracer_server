#include "acceptor.h"

Acceptor::Acceptor(boost::asio::io_service& ios, unsigned short port_number, const QSqlDatabase& db)
    : m_ios(ios),
      m_acceptor(m_ios, boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::any(), port_number)),
      db(db)
{}

void Acceptor::start()
{
    m_acceptor.listen();
    init_accept();
}

void Acceptor::init_accept()
{
    std::shared_ptr<boost::asio::ip::tcp::socket> socket(new boost::asio::ip::tcp::socket(m_ios));
    m_acceptor.async_accept(*socket.get(), [this, socket](const boost::system::error_code& error)
    {
        on_accept(error, socket);
    });
}

void Acceptor::on_accept(const boost::system::error_code& ec, std::shared_ptr<boost::asio::ip::tcp::socket> socket)
{
    if(ec.value() == 0) {
        (new Service(socket, db))->start_handling();
    } else {
        socket->close();
    }
    init_accept();
}
