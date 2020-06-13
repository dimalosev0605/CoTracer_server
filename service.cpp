#include "service.h"

const QString path_to_avatars = "/home/dima/Documents/Qt_projects/mhc_server_2_avatars/";
const QString date_format = "dd.MM.yy";

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
}

void Service::on_request_received(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    if(ec.value() != 0) {
        qDebug() << "Error occured! Message: " << ec.message().data();
        on_finish();
        return;
    } else {
        process_request(bytes_transferred);

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

void Service::process_request(std::size_t bytes_transferred)
{
    std::string data = std::string{boost::asio::buffers_begin(m_request.data()),
                                   boost::asio::buffers_begin(m_request.data()) + bytes_transferred - Protocol_keys::end_of_message.size()};
    auto req_j_doc = QJsonDocument::fromJson(data.c_str());
    m_request.consume(bytes_transferred);

    if(!req_j_doc.isEmpty()) {
        auto req_j_obj = req_j_doc.object();
        auto req_j_map = req_j_obj.toVariantMap();
        Request_code req_code = (Request_code)req_j_map[Protocol_keys::request_code].toInt();
        QJsonObject res_j_obj;

        process_data(req_j_map, res_j_obj, req_code);

        QJsonDocument res_j_doc(res_j_obj);
        m_response = res_j_doc.toJson().data() + Protocol_keys::end_of_message.toStdString();
    }
    else {
        on_finish();
    }
}

void Service::process_data(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj, Request_code req_code)
{
    switch (req_code) {

    case Request_code::sign_up: {
        process_sign_up_request(request_map, response_j_obj);
        break;
    }
    case Request_code::sign_in: {
        process_sign_in_request(request_map, response_j_obj);
        break;
    }
    case Request_code::add_contact: {
        process_add_contact_request(request_map, response_j_obj);
        break;
    }
    case Request_code::remove_contact: {
        process_remove_contact_request(request_map, response_j_obj);
        break;
    }
    case Request_code::fetch_stat_for_14_days: {
        process_fetch_stat_for_14_days_request(request_map, response_j_obj);
        break;
    }
    case Request_code::fetch_contacts: {
        process_fetch_contacts_request(request_map, response_j_obj);
        break;
    }
    case Request_code::change_avatar: {
        process_change_avatar_request(request_map, response_j_obj);
        break;
    }
    case Request_code::set_default_avatar: {
        process_set_default_avatar_request(request_map, response_j_obj);
        break;
    }
    case Request_code::change_password: {
        process_change_password_request(request_map, response_j_obj);
        break;
    }

    }
}


void Service::process_sign_up_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj)
{
    QString user_nickname = request_map[Protocol_keys::user_nickname].toString();
    QString user_password = request_map[Protocol_keys::user_password].toString();
    Response_code res_code;

    QString str_qry = QString("insert into main (user_name, user_password) values ('%1', '%2')")
            .arg(user_nickname).arg(user_password);

    if(m_qry.exec(str_qry))
    {
        str_qry = QString("create table %1 (date varchar(8) not null, contacts text)").arg(user_nickname);
        if(m_qry.exec(str_qry))
        {
            if(fill_table(m_qry, user_nickname))
            {
                res_code = Response_code::success_sign_up;
            }
            else
            {
                str_qry = QString("drop table %1").arg(user_nickname);
                m_qry.exec(str_qry);

                str_qry = QString("delete from main where user_name = '%1'").arg(user_nickname);
                m_qry.exec(str_qry);

                res_code = Response_code::internal_server_error;
            }
        }
        else
        {
            str_qry = QString("delete from main where user_name = '%1'").arg(user_nickname);
            m_qry.exec(str_qry);
            res_code = Response_code::internal_server_error;
        }
    }
    else
    {
        res_code = Response_code::sign_up_failure;
    }

    response_j_obj.insert(Protocol_keys::response_code, (int)res_code);
}

void Service::process_sign_in_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj)
{
    QString user_nickname = request_map[Protocol_keys::user_nickname].toString();
    QString user_password = request_map[Protocol_keys::user_password].toString();
    Response_code res_code;

    QString str_qry = QString("select user_name, user_password from main where user_name = '%1' and user_password = '%2'")
            .arg(user_nickname).arg(user_password);

    if(m_qry.exec(str_qry))
    {
        if(m_qry.size())
        {
            res_code = Response_code::success_sign_in;
            // fetch avatar
            QString file_path = path_to_avatars + user_nickname;
            QFile file(file_path);
            if(file.open(QIODevice::ReadOnly)) {
                QByteArray b_arr = file.readAll();
                QByteArray base_64_arr = b_arr.toBase64();
                response_j_obj.insert(Protocol_keys::avatar_data, QString::fromLatin1(base_64_arr));
            }
            // end fetch avatar
        }
        else
        {
            res_code = Response_code::sign_in_failure;
        }
    }
    else
    {
        res_code = Response_code::internal_server_error;
    }

    response_j_obj.insert(Protocol_keys::response_code, (int)res_code);
}

void Service::process_add_contact_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj)
{
    QString user_nickname = request_map[Protocol_keys::user_nickname].toString();
    QString contact_nickname = request_map[Protocol_keys::contact_nickname].toString();
    QString contact_time = request_map[Protocol_keys::contact_time].toString();
    QString contact_date = request_map[Protocol_keys::contact_date].toString();
    bool is_contact_avatar_cached = request_map[Protocol_keys::is_contact_avatar_cached].toBool();
    Response_code res_code;

    QString str_qry = QString("select from main where user_name = '%1'").arg(contact_nickname);

    if(m_qry.exec(str_qry))
    {
        if(m_qry.size())
        {
            str_qry = QString("update %1 set contacts = contacts || ',%2-%3' where date = '%4'")
                    .arg(user_nickname).arg(contact_nickname).arg(contact_time).arg(contact_date);
            if(m_qry.exec(str_qry)) {
                res_code = Response_code::success_contact_adding;
                if(!is_contact_avatar_cached) {
                    QFile contact_avatar(path_to_avatars + contact_nickname);
                    if(contact_avatar.open(QIODevice::ReadOnly)) {
                        QByteArray avatar_b_arr = contact_avatar.readAll();
                        QByteArray base_64_avatar = avatar_b_arr.toBase64();
                        response_j_obj.insert(Protocol_keys::avatar_data, QString::fromLatin1(base_64_avatar));
                        response_j_obj.insert(Protocol_keys::avatar_downloaded_date_time,
                                              QDateTime::currentDateTime().toString(Protocol_keys::avatar_downloaded_date_time_format));
                    }
                }
            } else {
                res_code = Response_code::internal_server_error;
            }
        }
        else
        {
            res_code = Response_code::such_contact_not_exists;
        }
    }
    else
    {
        res_code = Response_code::internal_server_error;
    }

    response_j_obj.insert(Protocol_keys::response_code, (int)res_code);
}

void Service::process_remove_contact_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj)
{
    QString user_nickname = request_map[Protocol_keys::user_nickname].toString();
    QString contact_date = request_map[Protocol_keys::contact_date].toString();
    Response_code res_code;

    QString str_qry = QString("select contacts from %1 where date = '%2'").arg(user_nickname).arg(contact_date);

    if(m_qry.exec(str_qry))
    {
        QString cell_value;

        while(m_qry.next()) {
            cell_value = m_qry.value(0).toString();
        }

        QString contact_nickname = request_map[Protocol_keys::contact_nickname].toString();
        QString contact_time = request_map[Protocol_keys::contact_time].toString();

        QString find_contact = "," + contact_nickname + "-" + contact_time;

        cell_value = cell_value.remove(find_contact);

        str_qry = QString("update %1 set contacts = '%2' where date = '%3'")
                .arg(user_nickname).arg(cell_value).arg(contact_date);

        if(m_qry.exec(str_qry)) {
            res_code = Response_code::success_contact_deletion;
        } else {
            res_code = Response_code::internal_server_error;
        }
    }
    else
    {
        res_code = Response_code::internal_server_error;
    }

    response_j_obj.insert(Protocol_keys::response_code, (int)res_code);
}

void Service::process_fetch_stat_for_14_days_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj)
{
    QString user_nickname = request_map[Protocol_keys::user_nickname].toString();
    Response_code res_code;

    QString str_qry = QString("select date from %1").arg(user_nickname);

    if(m_qry.exec(str_qry)) {

        QVector<QString> dates;
        while(m_qry.next()) {
            dates.push_back(m_qry.value(0).toString());
        }
        if(dates.isEmpty()) {
            res_code = Response_code::internal_server_error;
            response_j_obj.insert(Protocol_keys::response_code, (int)res_code);
            return;
        }

        std::set<QString> unique_contacts;
        QVector<std::tuple<QString, int>> stat_for_14_days;

        for(int i = 0; i < dates.size(); ++i) {
            unique_contacts.insert(user_nickname);
            if(!count_contacts_recursively(dates[i], user_nickname, unique_contacts)) {
                res_code = Response_code::internal_server_error;
                response_j_obj.insert(Protocol_keys::response_code, (int)res_code);
                return;
            }
            unique_contacts.erase(user_nickname);
            stat_for_14_days.push_back(std::make_tuple(dates[i], unique_contacts.size()));

            unique_contacts.clear();
        }

        QJsonArray j_stat_arr;
        for(int i = 0; i < stat_for_14_days.size(); ++i) {
            QJsonObject day_stat;
            day_stat.insert(Protocol_keys::stat_date, std::get<0>(stat_for_14_days[i]));
            day_stat.insert(Protocol_keys::quantity_of_contacts, std::get<1>(stat_for_14_days[i]));
            j_stat_arr.append(day_stat);
        }
        response_j_obj.insert(Protocol_keys::statistic_for_14_days, j_stat_arr);

        res_code = Response_code::success_fetching_stat_for_14_days;

    } else {
        res_code = Response_code::internal_server_error;
    }

    response_j_obj.insert(Protocol_keys::response_code, (int)res_code);
}

void Service::process_fetch_contacts_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj)
{
    QString contact_nickname = request_map[Protocol_keys::contact_nickname].toString();
    QString contact_date = request_map[Protocol_keys::contact_date].toString();

    // make vec of cached avars
    auto cached_avatars = request_map[Protocol_keys::cached_avatars].toJsonArray();
    std::vector<std::tuple<QString, QDateTime>> cached_avatars_v;
    for(int i = 0; i < cached_avatars.size(); ++i) {
        auto obj = cached_avatars[i].toObject();
        auto map = obj.toVariantMap();
        QString contact_nickname = map[Protocol_keys::contact_nickname].toString();
        QDateTime avatar_downloaded_time = QDateTime::fromString(map[Protocol_keys::avatar_downloaded_date_time].toString(),
                Protocol_keys::avatar_downloaded_date_time_format);
        cached_avatars_v.push_back(std::make_tuple(contact_nickname, avatar_downloaded_time));
    }
    // end make vec of cached avars

    Response_code res_code;

    QString str_qry = QString("select contacts from %1 where date = '%2'").arg(contact_nickname).arg(contact_date);

    QString m_cell_value;

    if(m_qry.exec(str_qry)) {

        while(m_qry.next()) {
            m_cell_value = m_qry.value(0).toString();
        }

        res_code = Response_code::success_fetching_contacts;

        auto pairs_list = m_cell_value.split(',', QString::SkipEmptyParts);

        QVector<QJsonObject> contacts_list;
        contacts_list.reserve(pairs_list.size());

        for(int i = 0; i < pairs_list.size(); ++i) {
            auto pair = pairs_list[i].split('-', QString::SkipEmptyParts);
            QJsonObject contact;
            contact.insert(Protocol_keys::contact_nickname, pair[0]);
            contact.insert(Protocol_keys::contact_time, pair[1]);

            //
            QFileInfo avatar_file_info(path_to_avatars + pair[0]);
            QDateTime uploaded_on_server_time = avatar_file_info.lastModified();
            auto iter = std::find_if(cached_avatars_v.begin(), cached_avatars_v.end(), [&](const std::tuple<QString, QDateTime>& i)
            {
                return pair[0] == std::get<0>(i);
            });
            if(iter != cached_avatars_v.end())
            {
                // if avatar in cache, but it was deleted
                if(!avatar_file_info.exists()) {
                    contact.insert(Protocol_keys::avatar_downloaded_date_time, Protocol_keys::deleted_avatar);
                    contacts_list.push_back(contact);
                    continue;
                }

                // if cached -> check time
                qDebug() << "Avatar " << pair[0] << " in cache, check time:";
                if(uploaded_on_server_time > std::get<1>(*iter))
                {
                    qDebug() << "Time expired, insert data";
                    // if expired -> insert avatar data and downloaded time
                    QFile avatar_file(path_to_avatars + pair[0]);
                    if(avatar_file.open(QIODevice::ReadOnly)) {
                        QByteArray avatar_b_arr = avatar_file.readAll();
                        QByteArray base_64_avatar = avatar_b_arr.toBase64();
                        contact.insert(Protocol_keys::avatar_data, QString::fromLatin1(base_64_avatar));
                        contact.insert(Protocol_keys::avatar_downloaded_date_time,
                                       QDateTime::currentDateTime().toString(Protocol_keys::avatar_downloaded_date_time_format));
                    }
                }
                else {
                    qDebug() << "Time not expired!";
                }
            }
            else
            {
                qDebug() << "Avatar " << pair[0] << " not cached, Insert data";
                // if not cached insert data and downloaded time
                QFile avatar_file(path_to_avatars + pair[0]);
                if(avatar_file.open(QIODevice::ReadOnly)) {
                    QByteArray avatar_b_arr = avatar_file.readAll();
                    QByteArray base_64_avatar = avatar_b_arr.toBase64();
                    contact.insert(Protocol_keys::avatar_data, QString::fromLatin1(base_64_avatar));
                    contact.insert(Protocol_keys::avatar_downloaded_date_time,
                                   QDateTime::currentDateTime().toString(Protocol_keys::avatar_downloaded_date_time_format));
                }
            }
            //

            contacts_list.push_back(contact);
        }

        QJsonArray j_arr_of_contacts;
        for(int i = 0; i < contacts_list.size(); ++i) {
            j_arr_of_contacts.append(contacts_list[i]);
        }

        response_j_obj.insert(Protocol_keys::contact_list, j_arr_of_contacts);

    } else {
        res_code = Response_code::internal_server_error;
    }

    response_j_obj.insert(Protocol_keys::response_code, (int)res_code);
}

void Service::process_change_avatar_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj)
{
    QString user_nickname = request_map[Protocol_keys::user_nickname].toString();
    QString str_avatar = request_map[Protocol_keys::avatar_data].toString();
    QByteArray avatar_data = QByteArray::fromBase64(str_avatar.toLatin1());
    Response_code res_code;

    QString avatar_name(path_to_avatars + user_nickname);
    QFile file(avatar_name);
    if(file.open(QIODevice::WriteOnly)) {
        auto must_be_written = avatar_data.size();
        auto written = file.write(avatar_data);
        if(written != must_be_written) {
            res_code = Response_code::internal_server_error;
            response_j_obj.insert(Protocol_keys::response_code, (int)res_code);
            return;
        }
        res_code = Response_code::success_avatar_changing;
    }
    else {
        res_code = Response_code::internal_server_error;
    }

    response_j_obj.insert(Protocol_keys::response_code, (int)res_code);
}

void Service::process_set_default_avatar_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj)
{
    auto user_nickname = request_map[Protocol_keys::user_nickname].toString();
    Response_code res_code;

    QString avatar_path = path_to_avatars + user_nickname;
    QFile file(avatar_path);
    if(file.remove()) {
        res_code = Response_code::success_setting_default_avatar;
    }
    else {
        res_code = Response_code::internal_server_error;
    }
    response_j_obj.insert(Protocol_keys::response_code, (int)res_code);
}

void Service::process_change_password_request(const QMap<QString, QVariant>& request_map, QJsonObject& response_j_obj)
{
    QString user_nickname = request_map[Protocol_keys::user_nickname].toString();
    QString user_password = request_map[Protocol_keys::user_password].toString();
    Response_code res_code;

    QString str_qry = QString("update main set user_password = '%1' where user_name = '%2'")
            .arg(user_password).arg(user_nickname);

    if(m_qry.exec(str_qry)) {
        res_code = Response_code::success_password_changing;
    } else {
        res_code = Response_code::internal_server_error;
    }

    response_j_obj.insert(Protocol_keys::response_code, (int)res_code);
}

bool Service::count_contacts_recursively(const QString& date, const QString& nick, std::set<QString>& m_unique_contacts)
{
    QString str_qry = QString("select contacts from %1 where date = '%2'").arg(nick).arg(date);
    m_qry.exec(str_qry);

    QString cell_value;
    while(m_qry.next()) {
        cell_value = m_qry.value(0).toString();
    }
    auto list_pairs = cell_value.split(',', QString::SkipEmptyParts);

    QVector<QString> contact_list;
    for(int i = 0; i < list_pairs.size(); ++i) {
        auto pair = list_pairs[i].split('-', QString::SkipEmptyParts);
        contact_list.push_back(pair[0]);
    }

    for(int i = 0; i < contact_list.size(); ++i) {
        if(m_unique_contacts.find(contact_list[i]) == m_unique_contacts.end()) {
            m_unique_contacts.insert(contact_list[i]);
            if(!count_contacts_recursively(date, contact_list[i], m_unique_contacts)) {
                return false;
            }
        }
    }

    return true;
}

bool Service::fill_table(QSqlQuery& qry, const QString& nickname)
{
    QDate curr_date = QDate::currentDate();
    const int count_of_days = 14;
    for(int i = 0; i < count_of_days; ++i) {
        QString str_date = curr_date.toString(date_format);
        QString str_qry = QString("insert into %1 (date, contacts) values('%2', '')")
                .arg(nickname).arg(str_date);
        curr_date = curr_date.addDays(-1);
        if(!qry.exec(str_qry)) {
            return false;
        }
    }
    return true;
}
