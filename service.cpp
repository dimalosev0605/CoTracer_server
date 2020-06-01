#include "service.h"

Service::Service(std::shared_ptr<boost::asio::ip::tcp::socket> socket, const QSqlDatabase& db)
    : m_socket(socket),
      m_qry(db)
{}

void Service::start_handling()
{
    boost::asio::async_read_until(*m_socket.get(), m_request, Protocol_keys::end_of_message.toStdString(),
                                  [this](const boost::system::error_code& ec, std::size_t bytes_transferred)
    {
        on_request_received(ec, bytes_transferred);
    });
}

void Service::on_finish()
{
    qDebug() << this << " destroyed";
    delete this;
}

void Service::cleanup()
{
    m_response.clear();
    m_user_nickname.clear();
    m_user_password.clear();
    m_contact_nickname.clear();
    m_contact_time.clear();
    m_contact_date.clear();
    m_cell_value.clear();
    m_stat_for_14_days.clear();
    m_avatar_data.clear();
}

void Service::on_request_received(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    if(ec.value() != 0) {
        qDebug() << "Error occured! Message: " << ec.message().data();
        on_finish();
        return;
    } else {
        parse_request(bytes_transferred);
        m_res_code = process_data();
        create_response();

        boost::asio::async_write(*m_socket.get(), boost::asio::buffer(m_response),
                                 [this](const boost::system::error_code& ec, std::size_t bytes_transferred)
        {
            on_response_sent(ec);
        });
    }
}

void Service::on_response_sent(const boost::system::error_code& ec)
{
    if(ec.value() == 0) {
        cleanup();
        start_handling();
    } else {
        on_finish();
    }
}

void Service::parse_request(std::size_t bytes_transferred)
{
    std::string data = std::string{boost::asio::buffers_begin(m_request.data()),
                                   boost::asio::buffers_begin(m_request.data()) + bytes_transferred - Protocol_keys::end_of_message.size()};
    auto j_doc = QJsonDocument::fromJson(data.c_str());
    m_request.consume(bytes_transferred);

    if(!j_doc.isEmpty()) {
        auto j_obj = j_doc.object();
        auto j_map = j_obj.toVariantMap();
        m_req_code = (Request_code)j_map[Protocol_keys::request_code].toInt();

        m_user_nickname = j_map[Protocol_keys::user_nickname].toString().toStdString();
        m_user_password = j_map[Protocol_keys::user_password].toString().toStdString();
        m_contact_nickname = j_map[Protocol_keys::contact_nickname].toString().toStdString();
        m_contact_time = j_map[Protocol_keys::contact_time].toString().toStdString();
        m_contact_date = j_map[Protocol_keys::contact_date].toString().toStdString();

        QString str_avatar = j_map[Protocol_keys::avatar_data].toString();
        m_avatar_data = QByteArray::fromBase64(str_avatar.toLatin1());
    }

}

Service::Response_code Service::process_data()
{
    switch (m_req_code) {

    case Request_code::sign_up: {
        return process_sign_up_request();
    }
    case Request_code::sign_in: {
        return process_sign_in_request();
    }
    case Request_code::add_contact: {
        return process_add_contact_request();
    }
    case Request_code::remove_contact: {
        return process_remove_contact_request();
    }
    case Request_code::fetch_stat_for_14_days: {
        return process_fetch_stat_for_14_days_request();
    }
    case Request_code::fetch_contacts: {
        return process_fetch_contacts_request();
    }
    case Request_code::change_avatar: {
        return process_change_avatar_request();
    }
    case Request_code::change_password: {
        return process_change_password_request();
    }

    }
}

void Service::create_response()
{
    QJsonObject j_obj;
    j_obj.insert(Protocol_keys::response_code, (int)m_res_code);

    if(m_res_code == Response_code::success_fetching_stat_for_14_days) {
        insert_stats_arr(j_obj);
    }

    if(m_res_code == Response_code::success_fetching_contacts) {
        insert_arrs_of_contacts_in_jobj(j_obj);
    }

    if(m_res_code == Response_code::success_sign_in) {
        fetch_avatar();
        insert_avatar(j_obj);
    }

    QJsonDocument j_doc(j_obj);

    std::string s = j_doc.toJson().data();
    s += Protocol_keys::end_of_message.toStdString();
    m_response = s;
}

Service::Response_code Service::process_sign_up_request()
{
    QString str_qry = QString("insert into main (user_name, user_password) values ('%1', '%2')")
            .arg(QString::fromStdString(m_user_nickname)).arg(QString::fromStdString(m_user_password));

    if(m_qry.exec(str_qry)) {

        str_qry = QString("create table %1 (date varchar(8) not null, registered_contacts text, unregistered_contacts text)")
                .arg(QString::fromStdString(m_user_nickname));

        if(m_qry.exec(str_qry)) {

            if(fill_table(m_qry, QString::fromStdString(m_user_nickname))) {
                return Response_code::success_sign_up;
            } else {
                str_qry = QString("drop table %1").arg(QString::fromStdString(m_user_nickname));
                m_qry.exec(str_qry);

                str_qry = QString("delete from main where user_name = '%1'")
                        .arg(QString::fromStdString(m_user_nickname));
                m_qry.exec(str_qry);

                return Response_code::internal_server_error;
            }

        } else {
            str_qry = QString("delete from main where user_name = '%1'")
                    .arg(QString::fromStdString(m_user_nickname));
            m_qry.exec(str_qry);
            return Response_code::internal_server_error;
        }

    } else {
        return Response_code::sign_up_failure;
    }
}

Service::Response_code Service::process_sign_in_request()
{
    QString str_qry = QString("select user_name, user_password from main where user_name = '%1' and user_password = '%2'")
            .arg(QString::fromStdString(m_user_nickname)).arg(QString::fromStdString(m_user_password));

    if(m_qry.exec(str_qry)) {
        if(m_qry.size()) {
            return  Response_code::success_sign_in;
        } else {
            return Response_code::sign_in_failure;
        }
    } else {
        return Response_code::internal_server_error;
    }
}

Service::Response_code Service::process_add_contact_request()
{
    QString str_qry = QString("select from main where user_name = '%1'")
            .arg(QString::fromStdString(m_contact_nickname));

    if(m_qry.exec(str_qry)) {

        if(m_qry.size()) {

            str_qry = QString("update %1 set registered_contacts = registered_contacts || ',%2-%3' where date = '%4'")
                    .arg(QString::fromStdString(m_user_nickname)).arg(QString::fromStdString(m_contact_nickname))
                    .arg(QString::fromStdString(m_contact_time)).arg(QString::fromStdString(m_contact_date));

            if(m_qry.exec(str_qry)) {
                return Response_code::success_contact_adding;
            } else {
                return Response_code::internal_server_error;
            }

        } else {
            return Response_code::such_contact_not_exists;
        }

    } else {
        return Response_code::internal_server_error;
    }
}

Service::Response_code Service::process_remove_contact_request()
{
    QString str_qry = QString("select registered_contacts from %1 where date = '%2'")
            .arg(QString::fromStdString(m_user_nickname)).arg(QString::fromStdString(m_contact_date));

    qDebug() << "qry: " << str_qry;


    if(m_qry.exec(str_qry)) {

        while(m_qry.next()) {
            m_cell_value = m_qry.value(0).toString();
        }

        QString find_contact = "," + QString::fromStdString(m_contact_nickname)
                               + "-" + QString::fromStdString(m_contact_time);

        auto old_str = m_cell_value;
        m_cell_value = m_cell_value.remove(find_contact);

        str_qry = QString("update %1 set registered_contacts = '%2' where date = '%3'")
                .arg(QString::fromStdString(m_user_nickname)).arg(m_cell_value)
                .arg(QString::fromStdString(m_contact_date));

        if(m_qry.exec(str_qry)) {
            return Response_code::success_contact_deletion;
        } else {
            return Response_code::internal_server_error;
        }

    } else {
        return Response_code::internal_server_error;
    }
}

Service::Response_code Service::process_fetch_stat_for_14_days_request()
{
    QString str_qry = QString("select date from %1").arg(QString::fromStdString(m_user_nickname));

    if(m_qry.exec(str_qry)) {

        QVector<QString> dates;
        while(m_qry.next()) {
            dates.push_back(m_qry.value(0).toString());
        }
        if(dates.isEmpty()) return Response_code::internal_server_error;


        for(int i = 0; i < dates.size(); ++i) {
            m_unique_contacts.insert(QString::fromStdString(m_user_nickname));
            if(!count_contacts_recursively(dates[i], QString::fromStdString(m_user_nickname))) {
                m_unique_contacts.clear();
                return Response_code::internal_server_error;
            }

            m_unique_contacts.erase(QString::fromStdString(m_user_nickname));
            m_stat_for_14_days.push_back(std::make_tuple(dates[i], m_unique_contacts.size()));

            m_unique_contacts.clear();
        }

        return Response_code::success_fetching_stat_for_14_days;

    } else {
        return Response_code::internal_server_error;
    }
}

Service::Response_code Service::process_fetch_contacts_request()
{
    QString str_qry = QString("select registered_contacts from %1 where date = '%2'")
            .arg(QString::fromStdString(m_contact_nickname)).arg(QString::fromStdString(m_contact_date));

    if(m_qry.exec(str_qry)) {

        while(m_qry.next()) {
            m_cell_value = m_qry.value(0).toString();
        }

        return Response_code::success_fetching_contacts;
    } else {
        return Response_code::internal_server_error;
    }
}

Service::Response_code Service::process_change_avatar_request()
{
    QString avatar_name("/home/dima/Documents/Qt_projects/mhc_server_2_avatars/" + QString::fromStdString(m_user_nickname));
    QFile file(avatar_name);
    if(file.open(QIODevice::WriteOnly)) {
        auto must_be_written = m_avatar_data.size();
        auto written = file.write(m_avatar_data);
        if(written != must_be_written) {
            file.close();
            return Response_code::internal_server_error;
        }

        file.close();
        return Response_code::success_avatar_changing;
    }

    return Response_code::internal_server_error;
}

Service::Response_code Service::process_change_password_request()
{
    QString str_qry = QString("update main set user_password = '%1' where user_name = '%2'")
            .arg(QString::fromStdString(m_user_password)).arg(QString::fromStdString(m_user_nickname));

    if(m_qry.exec(str_qry)) {
        return Response_code::success_password_changing;
    } else {
        return Response_code::internal_server_error;
    }
}

void Service::fetch_avatar()
{
    QString file_path = "/home/dima/Documents/Qt_projects/mhc_server_2_avatars/" + QString::fromStdString(m_user_nickname);
    QFile file(file_path);
    if(file.open(QIODevice::ReadOnly)) {
        QByteArray b_arr = file.readAll();
        m_avatar_data = b_arr.toBase64();
    }
}

bool Service::count_contacts_recursively(const QString& date, const QString& nick)
{
    QString str_qry = QString("select registered_contacts from %1 where date = '%2'").arg(nick).arg(date);
    m_qry.exec(str_qry);
    while(m_qry.next()) {
        m_cell_value = m_qry.value(0).toString();
    }
    auto reg_list_pairs = m_cell_value.split(',', QString::SkipEmptyParts);

    QVector<QString> reg_contacts_list;
    for(int i = 0; i < reg_list_pairs.size(); ++i) {
        auto pair = reg_list_pairs[i].split('-', QString::SkipEmptyParts);
        reg_contacts_list.push_back(pair[0]);
    }

    for(int i = 0; i < reg_contacts_list.size(); ++i) {
        if(m_unique_contacts.find(reg_contacts_list[i]) == m_unique_contacts.end()) {
            m_unique_contacts.insert(reg_contacts_list[i]);
            if(!count_contacts_recursively(date, reg_contacts_list[i])) {
                return false;
            }
        }
    }

    return true;
}

void Service::insert_arr_of_contacts_in_jobj(QJsonObject& j_obj)
{
    QStringList pairs_list;

    pairs_list = m_cell_value.split(',', QString::SkipEmptyParts);

    QVector<QJsonObject> contacts_list;
    contacts_list.reserve(pairs_list.size());

    for(int i = 0; i < pairs_list.size(); ++i) {
        auto pair = pairs_list[i].split('-', QString::SkipEmptyParts);
        QJsonObject contact;
        contact.insert(Protocol_keys::contact_nickname, pair[0]);
        contact.insert(Protocol_keys::contact_time, pair[1]);
        contacts_list.push_back(contact);
    }

    QJsonArray j_arr_of_contacts;
    for(int i = 0; i < contacts_list.size(); ++i) {
        j_arr_of_contacts.append(contacts_list[i]);
    }

    j_obj.insert(Protocol_keys::contact_list, j_arr_of_contacts);
}

void Service::insert_arr_of_avatars_in_jobj(QJsonObject& j_obj)
{
    auto pairs = m_cell_value.split(',', QString::SkipEmptyParts);
    QVector<QString> reg_nicknames;

    for(int i = 0; i < pairs.size(); ++i) {
        auto pair = pairs[i].split('-', QString::SkipEmptyParts);
        reg_nicknames.push_back(pair.first());
    }

    if(reg_nicknames.isEmpty()) return;

    QString path_to_avatars_folder = "/home/dima/Documents/Qt_projects/mhc_server_2_avatars/";
    QByteArray avatar_b_arr;
    QByteArray base_64_avatar;
    QJsonArray avatars;

    for(int i = 0; i < reg_nicknames.size(); ++i) {

        QFile avatar_file(path_to_avatars_folder + reg_nicknames[i]);
        if(avatar_file.open(QIODevice::ReadOnly)) {
            avatar_b_arr = avatar_file.readAll();
            base_64_avatar = avatar_b_arr.toBase64();

            QJsonObject avatar;
            avatar.insert(Protocol_keys::contact_nickname, reg_nicknames[i]);
            avatar.insert(Protocol_keys::avatar_data, QString::fromLatin1(base_64_avatar));
            avatars.append(avatar);
        }
        else {
        }

    }

    j_obj.insert(Protocol_keys::avatar_list, avatars);
}

void Service::insert_arrs_of_contacts_in_jobj(QJsonObject& j_obj)
{
    insert_arr_of_contacts_in_jobj(j_obj);
    insert_arr_of_avatars_in_jobj(j_obj);
}

void Service::insert_stats_arr(QJsonObject& j_obj)
{
    QJsonArray j_stats_arr;

    for(int i = 0; i < m_stat_for_14_days.size(); ++i) {
        QJsonObject day_stat;
        day_stat.insert(Protocol_keys::stat_date, std::get<0>(m_stat_for_14_days[i]));
        day_stat.insert(Protocol_keys::quantity_of_contacts, std::get<1>(m_stat_for_14_days[i]));
        j_stats_arr.append(day_stat);
    }
    j_obj.insert(Protocol_keys::statistic_for_14_days, j_stats_arr);
}

void Service::insert_avatar(QJsonObject& j_obj)
{
    j_obj.insert(Protocol_keys::avatar_data, QString::fromLatin1(m_avatar_data));
}


bool Service::fill_table(QSqlQuery& qry, const QString& nickname)
{
    QDate curr_date = QDate::currentDate();
    const int count_of_days = 14;
    for(int i = 0; i < count_of_days; ++i) {
        QString str_date = curr_date.toString("dd.MM.yy");
        QString str_qry = QString("insert into %1 (date, registered_contacts, unregistered_contacts) values('%2', '', '')")
                .arg(nickname).arg(str_date);
        curr_date = curr_date.addDays(-1);
        if(!qry.exec(str_qry)) {
            return false;
        }
    }
    return true;
}
