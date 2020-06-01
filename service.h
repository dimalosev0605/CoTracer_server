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
#include <QFile>

#include <boost/asio.hpp>

#include <set>

namespace Protocol_keys {

const QString user_nickname = "user_nickname";
const QString user_password = "user_password";

const QString request_code = "request_code";
const QString response_code = "response_code";

const QString contact_nickname = "contact_nickname";
const QString contact_time = "contact_time";
const QString contact_date = "contact_date";
const QString contact_list = "contact_list";

const QString statistic_for_14_days = "statistic_for_14_days";
const QString quantity_of_contacts = "quantity_of_contacts";
const QString stat_date = "stat_date";

const QString avatar_data = "avatar_data";
const QString avatar_list = "avatar_list";

const QString end_of_message = "\r\n\r\n";

}

class Service
{
    enum class Request_code: int {
        sign_up,
        sign_in,
        add_contact,
        remove_contact,
        fetch_stat_for_14_days,
        fetch_contacts,
        change_avatar,
        change_password
    };

    enum class Response_code: int {
        internal_server_error,
        success_sign_up,
        sign_up_failure,
        success_sign_in,
        sign_in_failure,
        success_contact_adding,
        such_contact_not_exists,
        success_contact_deletion,
        success_fetching_stat_for_14_days,
        success_fetching_contacts,
        success_avatar_changing,
        success_password_changing
    };

    std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;

    std::string m_response;
    boost::asio::streambuf m_request;

    QSqlQuery m_qry;

    Request_code m_req_code;
    Response_code m_res_code;

    std::string m_user_nickname;
    std::string m_user_password;
    std::string m_contact_nickname;
    std::string m_contact_time;
    std::string m_contact_date;

    std::set<QString> m_unique_contacts;
    QVector<std::tuple<QString, int>> m_stat_for_14_days; // date, count of contacts

    QByteArray m_avatar_data;
    QString m_cell_value;

private:
    void on_finish();
    void cleanup();

    void on_request_received(const boost::system::error_code& ec, std::size_t bytes_transferred);
    void on_response_sent(const boost::system::error_code& ec);

    void parse_request(std::size_t bytes_transferred);
    Response_code process_data();
    void create_response();

    // process request functions
    Response_code process_sign_up_request();
    Response_code process_sign_in_request();
    Response_code process_add_contact_request();
    Response_code process_remove_contact_request();
    Response_code process_fetch_stat_for_14_days_request();
    Response_code process_fetch_contacts_request();
    Response_code process_change_avatar_request();
    Response_code process_change_password_request();

    // miscellaneous functions
    void fetch_avatar();
    void insert_arr_of_contacts_in_j_obj(QJsonObject& j_obj);
    void insert_stat_arr_in_j_obj(QJsonObject& j_obj);

    bool fill_table(QSqlQuery& qry, const QString& nickname);
    bool count_contacts_recursively(const QString& date, const QString& nick);
    void insert_avatar(QJsonObject& j_obj);

public:
    Service(std::shared_ptr<boost::asio::ip::tcp::socket> sock, const QSqlDatabase& db);
    void start_handling();
};

#endif // SERVICE_H
