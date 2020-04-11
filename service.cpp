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

void Service::start_handling()
{
    boost::asio::async_read_until(*m_socket.get(), m_request, "\r\n\r\n",
                                  [this](const boost::system::error_code& ec, std::size_t bytes_transferred)
    {
        on_request_received(ec);
    });
}

void Service::on_request_received(const boost::system::error_code& ec)
{
    if(ec.value() != 0) {
        qDebug() << "Error occured! Message: " << ec.message().data();
        on_finish();
        return;
    } else {
        parse_request();
        process_data();
        m_response = create_response();

        boost::asio::async_write(*m_socket.get(), boost::asio::buffer(m_response),
                                 [this](const boost::system::error_code& ec, std::size_t bytes_transferred)
        {
            on_response_sent(ec);
        });
    }
}

void Service::parse_request()
{
    auto j_doc = QJsonDocument::fromJson((char*)m_request.data().data());
    m_request.consume(UINT_MAX);

    if(!j_doc.isEmpty()) {
        auto j_obj = j_doc.object();
        auto j_map = j_obj.toVariantMap();
        m_req_code = (Request_code)j_map["request"].toInt();
        m_nickname = j_map["nickname"].toString().toStdString();
        m_password = j_map["password"].toString().toStdString();
    }

}

void Service::process_data()
{
    switch (m_req_code) {
    case Request_code::sign_up: {
        QString str_qry = QString("insert into main (user_name, user_password) values ('%1', '%2')")
                .arg(QString::fromStdString(m_nickname)).arg(QString::fromStdString(m_password));
        if(m_qry.exec(str_qry)) {
            m_res_code = Response_code::success_sign_up;
        } else {
            m_res_code = Response_code::sign_up_failure;
        }
        break;
    }
    case Request_code::sign_in: {
        QString str_qry = QString("select user_name, user_password from main where user_name = '%1' and user_password = '%2'")
                .arg(QString::fromStdString(m_nickname)).arg(QString::fromStdString(m_password));
        if(m_qry.exec(str_qry)) {
            if(m_qry.size()) {
                m_res_code = Response_code::success_sign_in;
            } else {
                m_res_code = Response_code::sign_in_failure;
            }
        } else {
            m_res_code = Response_code::internal_server_error;
        }
        break;
    }
    }
}

const char* Service::create_response()
{
    QJsonObject j_obj;
    j_obj.insert("response", (int)m_res_code);
    QJsonDocument j_doc(j_obj);
    return j_doc.toJson().append("\r\n\r\n");
}

void Service::on_response_sent(const boost::system::error_code& ec)
{
    if(ec.value() == 0) {
        m_response.clear();
        start_handling();
    } else {
        on_finish();
    }
}
