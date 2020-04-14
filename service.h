#ifndef SERVICE_H
#define SERVICE_H

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>
#include <QThread>
#include <QDate>

#include <boost/asio.hpp>

class Service
{
    enum class Request_code: int {
        sign_up,
        sign_in,
        add_registered_user,
        add_unregistered_user,
        get_unregistered_contacts,
        get_registered_contacts
    };

    enum class Response_code: int {
        internal_server_error,
        success_sign_up,
        sign_up_failure,
        success_sign_in,
        sign_in_failure,
        success_adding,
        such_user_not_exists,
        unregistered_list,
        registered_list,
    };

    std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;

    std::string m_response;
    boost::asio::streambuf m_request;

    Request_code m_req_code;
    Response_code m_res_code;

    std::string m_nickname;
    std::string m_password;
    std::string m_contact_nickname;
    std::string m_contact_time;
    std::string m_contact_date;
    QString m_list;

    // create cleanup function !!!

    QSqlQuery m_qry;

private:
    void on_finish();

    void on_request_received(const boost::system::error_code& ec);
    void on_response_sent(const boost::system::error_code& ec);

    void parse_request();
    Response_code process_data();
    const char* create_response();

    Response_code process_sign_up_request();
    Response_code process_sign_in_request();
    Response_code process_add_unregistered_user_request();
    Response_code process_add_registered_user_request();
    Response_code process_get_unregistered_contacts();

    bool fill_table(QSqlQuery& qry, const QString& nickname);

public:
    Service(std::shared_ptr<boost::asio::ip::tcp::socket> sock, const QSqlDatabase& db);
    void start_handling();
};

#endif // SERVICE_H
