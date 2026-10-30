#include "core/GitStatusReader.h"

namespace mirante {

struct GitStatusReader::Private
{
};

GitStatusReader::GitStatusReader(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

GitStatusReader::~GitStatusReader() = default;

void GitStatusReader::setProgram(const QString &program) { Q_UNUSED(program) }

void GitStatusReader::setTimeout(int ms) { Q_UNUSED(ms) }

void GitStatusReader::read(const QString &path, QObject *receiver, std::function<void(const GitStatus &)> done)
{
    Q_UNUSED(path)
    Q_UNUSED(receiver)
    Q_UNUSED(done)
}

GitStatus GitStatusReader::parse(const QByteArray &output)
{
    Q_UNUSED(output)
    GitStatus status;
    status.result = GitStatus::Result::Ok;
    return status;
}

} // namespace mirante
