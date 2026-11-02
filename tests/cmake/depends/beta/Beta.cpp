#include "Alpha.h"

namespace mirante {
class ModuleRegistrar;
namespace modules::beta {
void registerModule(ModuleRegistrar &) { (void)alphaValue(); }
}
}
