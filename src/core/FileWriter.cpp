#include "core/FileWriter.h"

// Stubs written by T0 of the M3 plan. T3 implements them.

namespace mirante {

struct FileWriter::Private
{
};

FileWriter::FileWriter(const QString &, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

FileWriter::~FileWriter() = default;

void FileWriter::setDelay(int) {}
void FileWriter::schedule(const QString &, std::optional<QByteArray>) {}
void FileWriter::flush() {}
bool FileWriter::pending() const { return false; }
bool FileWriter::isOwnWrite(const QString &, const QByteArray &) const { return false; }

} // namespace mirante
