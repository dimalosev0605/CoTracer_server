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
#include <QDateTime>
#include <QFileInfo>
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

const QString cached_avatars = "cached_avatars";
const QString avatar_data = "avatar_data";
const QString avatar_downloaded_date_time = "avatar_downloaded_date_time";
const QString avatar_downloaded_date_time_format = "dd.MM.yyyy-hh:mm:ss";
const QString deleted_avatar = "000";

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
        set_default_avatar,
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
        success_password_changing,
        success_setting_default_avatar
    };

    std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;

    std::string m_response;
    boost::asio::streambuf m_request;

    QSqlQuery m_qry;

private:
    void on_finish();
    void cleanup();

    void on_request_received(const boost::system::error_code& ec, std::size_t bytes_transferred);
    void on_response_sent(const boost::system::error_code& ec);

    void process_request(std::size_t bytes_transferred);
    void process_data(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj, Request_code req_code);

    // process request functions
    void process_sign_up_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj);
    void process_sign_in_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj);
    void process_add_contact_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj);
    void process_remove_contact_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj);
    void process_fetch_stat_for_14_days_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj);
    void process_fetch_contacts_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj);
    void process_change_avatar_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj);
    void process_set_default_avatar_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj);
    void process_change_password_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj);

    // miscellaneous functions
    bool fill_table(QSqlQuery& qry, const QString& nickname);
    bool count_contacts_recursively(const QString& date, const QString& nick, std::set<QString>& m_unique_contacts);

public:
    Service(std::shared_ptr<boost::asio::ip::tcp::socket> sock, const QSqlDatabase& db);
    void start_handling();
};

#endif // SERVICE_H
