#include "service.h"

Service::Service(std::shared_ptr<boost::asio::ip::tcp::socket> socket, const QSqlDatabase& db)
    : m_socket(socket),
      m_qry(db)
{

}

void Service::on_finish()
{
    qDebug() << this << " destroyed";
    delete this;
}

void Service::cleanup()
{
    m_response.clear();

    m_nickname.clear();
    m_password.clear();
    m_contact_nickname.clear();
    m_contact_time.clear();
    m_contact_date.clear();
    m_list.clear();
    m_contacts_for_14_days.clear();
    m_reg_contacts_list.clear();
    m_unreg_contacts_list.clear();
    m_avatar.clear();
}

void Service::start_handling()
{
    boost::asio::async_read_until(*m_socket.get(), m_request, "\r\n\r\n",
                                  [this](const boost::system::error_code& ec, std::size_t bytes_transferred)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        on_request_received(ec, bytes_transferred);
    });
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

//        qDebug() << "m_response: " << m_response.c_str();

        boost::asio::async_write(*m_socket.get(), boost::asio::buffer(m_response),
                                 [this](const boost::system::error_code& ec, std::size_t bytes_transferred)
        {
            on_response_sent(ec);
        });
    }
}

void Service::parse_request(std::size_t bytes_transferred)
{
    std::string data = std::string{boost::asio::buffers_begin(m_request.data()),
                                boost::asio::buffers_begin(m_request.data()) + bytes_transferred - 4};
    auto j_doc = QJsonDocument::fromJson(data.c_str());
    m_request.consume(bytes_transferred);

//    qDebug() << "Raw data: " << QString::fromStdString(data);
    qDebug() << "bytes_transferred = " << bytes_transferred;

    if(!j_doc.isEmpty()) {
        auto j_obj = j_doc.object();
        auto j_map = j_obj.toVariantMap();
        m_req_code = (Request_code)j_map["request"].toInt();
        m_nickname = j_map["nickname"].toString().toStdString();
        m_password = j_map["password"].toString().toStdString();
        m_contact_nickname = j_map["contact"].toString().toStdString();
        m_contact_time = j_map["time"].toString().toStdString();
        m_contact_date = j_map["date"].toString().toStdString();
        QString str_avatar = j_map["avatar"].toString();
        m_avatar = QByteArray::fromBase64(str_avatar.toLatin1());
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

    case Request_code::add_unregistered_user: {
        return process_add_unregistered_user_request();
    }

    case Request_code::add_registered_user: {
        return process_add_registered_user_request();
    }

    case Request_code::remove_unregister_contact: {
        return process_remove_unregister_contact();
    }

    case Request_code::remove_register_contact: {
        return process_remove_registered_contact();
    }

    case Request_code::stats_for_14_days: {
        return process_stats_for_14_days();
    }

    case Request_code::get_contacts: {
        return process_get_contacts();
    }

    case Request_code::change_avatar: {
        return process_change_avatar();
    }

    case Request_code::get_my_avatar: {
        return process_get_my_avatar();
    }
    }
}

void Service::create_response()
{
    QJsonObject j_obj;
    j_obj.insert("response", (int)m_res_code);

//    if(m_res_code == Response_code::unregistered_list) {
//        insert_arr_of_contacts_in_jobj(j_obj, "unregistered_list");
//    }

//    if(m_res_code == Response_code::registered_list) {
//        insert_arr_of_contacts_in_jobj(j_obj, "registered_list");
//    }

    if(m_res_code == Response_code::success_fetch_stats_for_14_days) {
        insert_stats_arr(j_obj);
    }

    if(m_res_code == Response_code::contacts_list) {
        insert_arrs_of_contacts_in_jobj(j_obj);
    }

    if(m_res_code == Response_code::success_fetching_avatar) {
        insert_avatar(j_obj);
    }

    QJsonDocument j_doc(j_obj);
//    qDebug() << "Response: " << j_doc.toJson().data();

    std::string s = j_doc.toJson().data();
    s += "\r\n\r\n";
    m_response = s;
}

void Service::insert_avatar(QJsonObject& j_obj)
{
    j_obj.insert("avatar", QString::fromLatin1(m_avatar));
    qDebug() << "AVATAR INSERTED!";
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

Service::Response_code Service::process_sign_up_request()
{
    QString str_qry = QString("insert into main (user_name, user_password) values ('%1', '%2')")
            .arg(QString::fromStdString(m_nickname)).arg(QString::fromStdString(m_password));

    if(m_qry.exec(str_qry)) {

        str_qry = QString("create table %1 (date varchar(8) not null, registered_contacts text, unregistered_contacts text)")
                .arg(QString::fromStdString(m_nickname));

        if(m_qry.exec(str_qry)) {

            if(fill_table(m_qry, QString::fromStdString(m_nickname))) {
                return Response_code::success_sign_up;
            } else {
                str_qry = QString("drop table %1").arg(QString::fromStdString(m_nickname));
                m_qry.exec(str_qry);

                str_qry = QString("delete from main where user_name = '%1'")
                        .arg(QString::fromStdString(m_nickname));
                m_qry.exec(str_qry);

                return Response_code::internal_server_error;
            }

        } else {
            str_qry = QString("delete from main where user_name = '%1'")
                    .arg(QString::fromStdString(m_nickname));
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
            .arg(QString::fromStdString(m_nickname)).arg(QString::fromStdString(m_password));

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

Service::Response_code Service::process_add_unregistered_user_request()
{
    QString str_qry = QString("update %1 set unregistered_contacts = unregistered_contacts || ',%2-%3' where date = '%4'")
            .arg(QString::fromStdString(m_nickname)).arg(QString::fromStdString(m_contact_nickname))
            .arg(QString::fromStdString(m_contact_time)).arg(QString::fromStdString(m_contact_date));

    if(m_qry.exec(str_qry)) {
        return Response_code::success_adding;
    } else {
        return Response_code::internal_server_error;
    }
}

Service::Response_code Service::process_add_registered_user_request()
{
    QString str_qry = QString("select from main where user_name = '%1'")
            .arg(QString::fromStdString(m_contact_nickname));

    if(m_qry.exec(str_qry)) {

        if(m_qry.size()) {

            str_qry = QString("update %1 set registered_contacts = registered_contacts || ',%2-%3' where date = '%4'")
                    .arg(QString::fromStdString(m_nickname)).arg(QString::fromStdString(m_contact_nickname))
                    .arg(QString::fromStdString(m_contact_time)).arg(QString::fromStdString(m_contact_date));

            if(m_qry.exec(str_qry)) {
                return Response_code::success_adding;
            } else {
                return Response_code::internal_server_error;
            }

        } else {
            return Response_code::such_user_not_exists;
        }

    } else {
        return Response_code::internal_server_error;
    }
}


Service::Response_code Service::process_get_contacts()
{
    QString str_qry = QString("select registered_contacts from %1 where date = '%2'")
            .arg(QString::fromStdString(m_nickname)).arg(QString::fromStdString(m_contact_date));

    if(m_qry.exec(str_qry)) {

        while(m_qry.next()) {
            m_reg_contacts_list = m_qry.value(0).toString();
        }

        str_qry = QString("select unregistered_contacts from %1 where date = '%2'")
                .arg(QString::fromStdString(m_nickname)).arg(QString::fromStdString(m_contact_date));

        if(m_qry.exec(str_qry)) {

            while(m_qry.next()) {
                m_unreg_contacts_list = m_qry.value(0).toString();
            }

            return Response_code::contacts_list;

        } else {
            return Response_code::internal_server_error;
        }

    } else {
        return Response_code::internal_server_error;
    }

}

Service::Response_code Service::process_remove_unregister_contact()
{
    QString str_qry = QString("select unregistered_contacts from %1 where date = '%2'")
            .arg(QString::fromStdString(m_nickname)).arg(QString::fromStdString(m_contact_date));

    if(m_qry.exec(str_qry)) {

        while(m_qry.next()) {
            m_list = m_qry.value(0).toString();
        }

        QString find_contact = "," + QString::fromStdString(m_contact_nickname)
                               + "-" + QString::fromStdString(m_contact_time);

        qDebug() << "Before removing: " << m_list;
        auto old_str = m_list;
        m_list = m_list.remove(find_contact);
        qDebug() << "After removing: " << m_list;

        str_qry = QString("update %1 set unregistered_contacts = '%2' where date = '%3'")
                .arg(QString::fromStdString(m_nickname)).arg(m_list)
                .arg(QString::fromStdString(m_contact_date));

        if(m_qry.exec(str_qry)) {
            return Response_code::success_unregister_contact_deletion;
        } else {
            return Response_code::internal_server_error;
        }

    } else {
        return Response_code::internal_server_error;
    }

}

Service::Response_code Service::process_remove_registered_contact()
{
    QString str_qry = QString("select registered_contacts from %1 where date = '%2'")
            .arg(QString::fromStdString(m_nickname)).arg(QString::fromStdString(m_contact_date));

    qDebug() << "qry: " << str_qry;


    if(m_qry.exec(str_qry)) {

        while(m_qry.next()) {
            m_list = m_qry.value(0).toString();
        }

        QString find_contact = "," + QString::fromStdString(m_contact_nickname)
                               + "-" + QString::fromStdString(m_contact_time);

        qDebug() << "Before removing: " << m_list;
        auto old_str = m_list;
        m_list = m_list.remove(find_contact);
        qDebug() << "After removing: " << m_list;

        str_qry = QString("update %1 set registered_contacts = '%2' where date = '%3'")
                .arg(QString::fromStdString(m_nickname)).arg(m_list)
                .arg(QString::fromStdString(m_contact_date));

        if(m_qry.exec(str_qry)) {
            return Response_code::success_register_contact_deletion;
        } else {
            return Response_code::internal_server_error;
        }

    } else {
        return Response_code::internal_server_error;
    }
}

Service::Response_code Service::process_stats_for_14_days()
{
    QString str_qry = QString("select date from %1").arg(QString::fromStdString(m_nickname));

    if(m_qry.exec(str_qry)) {

        QVector<QString> dates;
        while(m_qry.next()) {
            dates.push_back(m_qry.value(0).toString());
        }
        if(dates.isEmpty()) return Response_code::internal_server_error;

//        qDebug() << "ALL dates:";
//        for(auto& i : dates) {
//            qDebug() << i;
//        }

        for(int i = 0; i < dates.size(); ++i) {
            m_unique_reg_contacts.insert(QString::fromStdString(m_nickname));
            if(!count_contacts_recursively(dates[i], QString::fromStdString(m_nickname))) {
                m_unique_reg_contacts.clear();
                m_unreg_contacts_counter = 0;
                return Response_code::internal_server_error;
            }

//            qDebug() << "STAT FOR DATE " << dates[i];
//            qDebug() << "Unreg contacts: " << m_unreg_counter;

            m_unique_reg_contacts.erase(QString::fromStdString(m_nickname));
//            qDebug() << "Reg contacts: " << m_unique_reg_contacts.size();
            m_contacts_for_14_days.push_back(std::make_tuple(dates[i], m_unique_reg_contacts.size(), m_unreg_contacts_counter));

            m_unreg_contacts_counter = 0;
            m_unique_reg_contacts.clear();
        }

//        qDebug() << "STATS:";
//        for(auto& i : m_contacts_for_14_days) {
//            qDebug() << std::get<0>(i) << " - " << std::get<1>(i) << " - " << std::get<2>(i);
//        }

        return Response_code::success_fetch_stats_for_14_days;

    } else {
        return Response_code::internal_server_error;
    }
}

Service::Response_code Service::process_change_avatar()
{
    QString avatar_name("/home/dima/Documents/Qt_projects/mhc_server_2_avatars/" + QString::fromStdString(m_nickname));
    QFile file(avatar_name);
    if(file.open(QIODevice::WriteOnly)) {
        auto must_be_written = m_avatar.size();
        auto written = file.write(m_avatar);
        if(written != must_be_written) {
            file.close();
            return Response_code::internal_server_error;
        }

        file.close();
        return Response_code::success_avatar_changing;
    }

    return Response_code::internal_server_error;
}

Service::Response_code Service::process_get_my_avatar()
{
    QString file_path = "/home/dima/Documents/Qt_projects/mhc_server_2_avatars/" + QString::fromStdString(m_nickname);
    QFile file(file_path);
    if(file.open(QIODevice::ReadOnly)) {
        QByteArray b_arr = file.readAll();
        m_avatar = b_arr.toBase64();
        file.close();
        if(m_avatar.isEmpty()) return Response_code::internal_server_error;

        qDebug() << "m_avatar.size()=" << m_avatar.size();

        return Response_code::success_fetching_avatar;
    }
    else {
        return Response_code::internal_server_error;
    }
}

bool Service::count_contacts_recursively(const QString& date, const QString& nick)
{
//    qDebug() << "in f(): date = " << date << ", nick = " << nick;

    QString str_qry = QString("select unregistered_contacts from %1 where date = '%2'").arg(nick).arg(date);
    m_qry.exec(str_qry);
    while(m_qry.next()) {
        m_list = m_qry.value(0).toString();
    }

    auto unreg_list = m_list.split(',', QString::SkipEmptyParts);
    m_unreg_contacts_counter += unreg_list.size();

//    qDebug() << "Unreg count = " << unreg_list.size();

    str_qry = QString("select registered_contacts from %1 where date = '%2'").arg(nick).arg(date);
    m_qry.exec(str_qry);
    while(m_qry.next()) {
        m_list = m_qry.value(0).toString();
    }
    auto reg_list_pairs = m_list.split(',', QString::SkipEmptyParts);

    QVector<QString> reg_contacts_list;
    for(int i = 0; i < reg_list_pairs.size(); ++i) {
        auto pair = reg_list_pairs[i].split('-', QString::SkipEmptyParts);
        reg_contacts_list.push_back(pair[0]);
    }

//    qDebug() << "Reg count = " << reg_contacts_list.size();
//    qDebug() << "REGISTRED CONTACTS LIST:";
//    for(auto& i : reg_contacts_list) {
//        qDebug() << i;
//    }

    for(int i = 0; i < reg_contacts_list.size(); ++i) {
        if(m_unique_reg_contacts.find(reg_contacts_list[i]) == m_unique_reg_contacts.end()) {
            m_unique_reg_contacts.insert(reg_contacts_list[i]);
            if(!count_contacts_recursively(date, reg_contacts_list[i])) {
                return false;
            }
        }
    }

    return true;
}

void Service::insert_arr_of_contacts_in_jobj(QJsonObject& j_obj, const QString& reg_or_unreg_list_key_word)
{
    QStringList pairs_list;
    if(reg_or_unreg_list_key_word == "unregistered_list") {
        pairs_list = m_unreg_contacts_list.split(',', QString::SkipEmptyParts);
    }
    if(reg_or_unreg_list_key_word == "registered_list") {
        pairs_list = m_reg_contacts_list.split(',', QString::SkipEmptyParts);
    }

    QVector<QJsonObject> contacts_list;
    contacts_list.reserve(pairs_list.size());

    for(int i = 0; i < pairs_list.size(); ++i) {
        auto pair = pairs_list[i].split('-', QString::SkipEmptyParts);
        QJsonObject contact;
        contact.insert("nickname", pair[0]);
        contact.insert("time", pair[1]);
        contacts_list.push_back(contact);
    }

    QJsonArray j_arr_of_contacts;
    for(int i = 0; i < contacts_list.size(); ++i) {
        j_arr_of_contacts.append(contacts_list[i]);
    }

    j_obj.insert(reg_or_unreg_list_key_word, j_arr_of_contacts);
}

void Service::insert_arr_of_avatars_in_jobj(QJsonObject& j_obj)
{
    auto pairs = m_reg_contacts_list.split(',', QString::SkipEmptyParts);
    QVector<QString> reg_nicknames;

    for(int i = 0; i < pairs.size(); ++i) {
        auto pair = pairs[i].split('-', QString::SkipEmptyParts);
        reg_nicknames.push_back(pair.first());
    }

    if(reg_nicknames.isEmpty()) return;
    qDebug() << "size = " << reg_nicknames.size();

    QString path_to_avatars_folder = "/home/dima/Documents/Qt_projects/mhc_server_2_avatars/";
    QByteArray avatar_b_arr;
    QByteArray base_64_avatar;
    QJsonArray avatars;

    for(int i = 0; i < reg_nicknames.size(); ++i) {

        QFile avatar_file(path_to_avatars_folder + reg_nicknames[i]);
        if(avatar_file.open(QIODevice::ReadOnly)) {
            qDebug() << avatar_file.fileName() << " Was inserted!";
            avatar_b_arr = avatar_file.readAll();
            base_64_avatar = avatar_b_arr.toBase64();

            QJsonObject avatar;
            avatar.insert("nickname", reg_nicknames[i]);
            avatar.insert("avatar", QString::fromLatin1(base_64_avatar));
            avatars.append(avatar);
        }
        else {
            qDebug() << avatar_file.fileName() << " was not opened";
        }

    }

    j_obj.insert("avatars", avatars);
}

void Service::insert_arrs_of_contacts_in_jobj(QJsonObject& j_obj)
{
    insert_arr_of_contacts_in_jobj(j_obj, "unregistered_list");
    insert_arr_of_contacts_in_jobj(j_obj, "registered_list");
    insert_arr_of_avatars_in_jobj(j_obj);
}

void Service::insert_stats_arr(QJsonObject& j_obj)
{
    QJsonArray j_stats_arr;

    for(int i = 0; i < m_contacts_for_14_days.size(); ++i) {
        QJsonObject day_stat;
        day_stat.insert("date", std::get<0>(m_contacts_for_14_days[i]));
        day_stat.insert("reg_qnt", std::get<1>(m_contacts_for_14_days[i]));
        day_stat.insert("unreg_qnt", std::get<2>(m_contacts_for_14_days[i]));
        j_stats_arr.append(day_stat);
    }

    j_obj.insert("stat", j_stats_arr);
}
