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

class Service
{
    enum class Request_code: int {
        sign_up,
        sign_in,
        add_registered_user,
        add_unregistered_user,
        remove_unregister_contact,
        remove_register_contact,
        stats_for_14_days,
        get_contacts,
        change_avatar,
        get_my_avatar
    };

    enum class Response_code: int {
        internal_server_error,
        success_sign_up,
        sign_up_failure,
        success_sign_in,
        sign_in_failure,
        success_adding,
        such_user_not_exists,
        success_unregister_contact_deletion,
        success_register_contact_deletion,
        success_fetch_stats_for_14_days,
        contacts_list,
        success_avatar_changing,
        success_fetching_avatar
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

    QString m_reg_contacts_list;
    QString m_unreg_contacts_list;

    std::set<QString> m_unique_reg_contacts;
    int m_unreg_contacts_counter = 0;
    QVector<std::tuple<QString, int , int>> m_contacts_for_14_days; // date, reg, unreg.

    QByteArray m_avatar;

    QSqlQuery m_qry;

private:
    void on_finish();
    void cleanup();

    void on_request_received(const boost::system::error_code& ec, std::size_t bytes_transferred);
    void on_response_sent(const boost::system::error_code& ec);

    void parse_request(std::size_t bytes_transferred);
    Response_code process_data();
    void create_response();

    Response_code process_sign_up_request();
    Response_code process_sign_in_request();
    Response_code process_add_unregistered_user_request();
    Response_code process_add_registered_user_request();
    Response_code process_remove_unregister_contact();
    Response_code process_remove_registered_contact();
    Response_code process_stats_for_14_days();
    Response_code process_get_contacts();
    Response_code process_change_avatar();
    Response_code process_get_my_avatar();
    void insert_arr_of_contacts_in_jobj(QJsonObject& j_obj, const QString& reg_or_unreg_list_key_word);
    void insert_arr_of_avatars_in_jobj(QJsonObject& j_obj);
    void insert_arrs_of_contacts_in_jobj(QJsonObject& j_obj);
    void insert_stats_arr(QJsonObject& j_obj);

    bool fill_table(QSqlQuery& qry, const QString& nickname);
    bool count_contacts_recursively(const QString& date, const QString& nick);
    void insert_avatar(QJsonObject& j_obj);

public:
    Service(std::shared_ptr<boost::asio::ip::tcp::socket> sock, const QSqlDatabase& db);
    void start_handling();
};

#endif // SERVICE_H
