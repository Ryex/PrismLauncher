#include "hematite-static/log.h"
#include <QByteArray>
#include <QMessageLogger>
#include <QString>

Q_LOGGING_CATEGORY(hematiteLogC, "launcher.hematite")

namespace prism {
namespace hematite {
namespace log {

void debug(rust::Str file, int32_t line, rust::Str function, rust::Str msg)
{
    QString qmsg = QString::fromUtf8(msg.data(), msg.size());
    QByteArray qfile(file.data(), file.size());
    QByteArray qfunction(function.data(), function.size());

    QMessageLogger(qfile.constData(), line, qfunction.constData(), hematiteLogC().categoryName()).debug().noquote() << qmsg;
}

void info(rust::Str file, int32_t line, rust::Str function, rust::Str msg)
{
    QString qmsg = QString::fromUtf8(msg.data(), msg.size());
    QByteArray qfile(file.data(), file.size());
    QByteArray qfunction(function.data(), function.size());

    QMessageLogger(qfile.constData(), line, qfunction.constData(), hematiteLogC().categoryName()).info().noquote() << qmsg;
}

void warn(rust::Str file, int32_t line, rust::Str function, rust::Str msg)
{
    QString qmsg = QString::fromUtf8(msg.data(), msg.size());
    QByteArray qfile(file.data(), file.size());
    QByteArray qfunction(function.data(), function.size());

    QMessageLogger(qfile.constData(), line, qfunction.constData(), hematiteLogC().categoryName()).warning().noquote() << qmsg;
}

void critical(rust::Str file, int32_t line, rust::Str function, rust::Str msg)
{
    QString qmsg = QString::fromUtf8(msg.data(), msg.size());
    QByteArray qfile(file.data(), file.size());
    QByteArray qfunction(function.data(), function.size());

    QMessageLogger(qfile.constData(), line, qfunction.constData(), hematiteLogC().categoryName()).critical().noquote() << qmsg;
}

}  // namespace log
}  // namespace hematite
}  // namespace prism
