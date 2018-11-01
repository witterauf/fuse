#include <fuse/LuaBaseLibrary.h>
#include <fuse/LuaFuseLibrary.h>
#include <fuse/graphics/GraphicsLuaLibrary.h>
#include <sol.hpp>

namespace fuse {

void openFuseLibrary(sol::this_state state)
{
    sol::state_view lua{state};
    lua["base"] = openBaseLibrary(state);
    lua["graphics"] = graphics::openGraphicsLibrary(state);
}

} // namespace fuse