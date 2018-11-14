#pragma once

#include "LuaDataFormatLoader.h"
#include <fuse/binary/PointerFormat.h>
#include <fuse/text/Table.h>

namespace fuse::binary {

class LuaRelativeOffsetFormatLoader : public LuaDataFormatLoader
{
public:
    auto load(const sol::table& format, sol::this_state state)
        -> std::optional<std::unique_ptr<RelativeOffsetFormat>>;

private:
    bool loadOffsetFormat(const sol::table& table, RelativeOffsetFormat& format);
    bool loadAddressFormat(const sol::table& table, RelativeOffsetFormat& format);
    bool loadBaseAddress(const sol::table& table, RelativeOffsetFormat& format);
    bool loadPointeeFormat(const sol::table& table, RelativeOffsetFormat& format);
};

} // namespace fuse::binary