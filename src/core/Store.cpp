// Stub from task T0. Task T8 implements this file.
#include "core/Store.h"

#include "core/Model.h"

namespace mirante::store {

std::vector<LoadProblem> load(const QString & /*dir*/, Model & /*model*/) { return {}; }

std::map<QString, QJsonObject> serialise(const Model & /*model*/) { return {}; }

} // namespace mirante::store
