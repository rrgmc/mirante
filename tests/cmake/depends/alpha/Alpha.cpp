#include "Alpha.h"

namespace mirante {
class ModuleRegistrar;
namespace modules::alpha {
void registerModule(ModuleRegistrar &) {}
}
}

int alphaValue()
{
    return 1;
}
