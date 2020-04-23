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
}

void Service::start_handling()
{
    boost::asio::async_read_until(*m_socket.get(), m_request, "\r\n\r\n",
                                  [this](const boost::system::error_code& ec, std::size_t bytes_transferred)
    {
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
        m_response = create_response();

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

    qDebug() << "Raw data: " << QString::fromStdString(data);

    if(!j_doc.isEmpty()) {
        auto j_obj = j_doc.object();
        auto j_map = j_obj.toVariantMap();
        m_req_code = (Request_code)j_map["request"].toInt();
        m_nickname = j_map["nickname"].toString().toStdString();
        m_password = j_map["password"].toString().toStdString();
        m_contact_nickname = j_map["contact"].toString().toStdString();
        m_contact_time = j_map["time"].toString().toStdString();
        m_contact_date = j_map["date"].toString().toStdString();
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

    case Request_code::get_unregistered_contacts: {
        return process_get_unregistered_contacts();
    }

    case Request_code::get_registered_contacts: {
        return process_get_registered_contacts();
    }

    case Request_code::remove_unregister_contact: {
        return process_remove_unregister_contact();
    }

    case Request_code::remove_register_contact: {
        return process_remove_registered_contact();
    }

    }
}

const char* Service::create_response()
{
    QJsonObject j_obj;
    j_obj.insert("response", (int)m_res_code);

    if(m_res_code == Response_code::unregistered_list) {
        insert_arr_of_contacts_in_jobj(j_obj, "unregistered_list");
    }

    if(m_res_code == Response_code::registered_list) {
        insert_arr_of_contacts_in_jobj(j_obj, "registered_list");
    }


    QJsonDocument j_doc(j_obj);
    qDebug() << "Response: " << j_doc.toJson().data();
    return j_doc.toJson().append("\r\n\r\n");
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

Service::Response_code Service::process_get_unregistered_contacts()
{
    QString str_qry = QString("select unregistered_contacts from %1 where date = '%2'")
            .arg(QString::fromStdString(m_nickname)).arg(QString::fromStdString(m_contact_date));

    if(m_qry.exec(str_qry)) {

        while(m_qry.next()) {
            m_list = m_qry.value(0).toString();
        }
        return Response_code::unregistered_list;

    } else {
        return Response_code::internal_server_error;
    }

}

Service::Response_code Service::process_get_registered_contacts()
{
    QString str_qry = QString("select registered_contacts from %1 where date = '%2'")
            .arg(QString::fromStdString(m_nickname)).arg(QString::fromStdString(m_contact_date));

    if(m_qry.exec(str_qry)) {

        while(m_qry.next()) {
            m_list = m_qry.value(0).toString();
        }
        return Response_code::registered_list;

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

void Service::insert_arr_of_contacts_in_jobj(QJsonObject& j_obj, const QString& reg_or_unreg_list_key_word)
{
    auto pairs_list = m_list.split(',', QString::SkipEmptyParts);

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
