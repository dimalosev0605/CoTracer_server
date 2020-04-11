#ifndef SERVICE_H
#define SERVICE_H

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>
#include <QThread>

#include <boost/asio.hpp>

class Service
{
    enum class Request_code: int {
        sign_up,
        sign_in
    };

    enum class Response_code: int {
        internal_server_error,
        success_sign_up,
        sign_up_failure,
        success_sign_in,
        sign_in_failure
    };

    std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
    std::string m_response;
    boost::asio::streambuf m_request;

    Request_code m_req_code;
    Response_code m_res_code;

    std::string m_nickname;
    std::string m_password;
    QSqlQuery m_qry;

private:
    void on_finish();
    void on_request_received(const boost::system::error_code& ec);
    void parse_request();
    void process_data();
    const char* create_response();
    void on_response_sent(const boost::system::error_code& ec);

public:
    Service(std::shared_ptr<boost::asio::ip::tcp::socket> sock, const QSqlDatabase& db);
    void start_handling();
};

#endif // SERVICE_H
