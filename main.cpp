#include <QCoreApplication>

#include "server.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Server server;
    server.start(1234, 2, "mhc_db", "localhost", "postgres", "4952211285", 5432);
    server.join_threads();

    return a.exec();
}
