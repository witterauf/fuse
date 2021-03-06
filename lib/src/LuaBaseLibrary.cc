#include <FuseLuaRuntime.h>
#include <fstream>
#include <fuse/Binary.h>
#include <fuse/Integers.h>
#include <fuse/LuaBaseLibrary.h>
#include <fuse/lua/Utilities.h>
#include <fuse/utilities/CsvReader.h>
#include <fuse/utilities/StringCollection.h>
#include <sol.hpp>

namespace fuse {

auto loadBinary(const std::string& filename) -> Binary
{
    return Binary::load(filename);
}

void saveBinary(const Binary& binary, const std::string& filename)
{
    binary.save(filename);
}

auto escapeCsvString(std::string input) -> std::string
{
    if (input.find(',', 0) == std::string::npos && input.find('\n', 0) == std::string::npos &&
        input.find('\r', 0) == std::string::npos)
    {
        return input;
    }
    else
    {
        // escape " by ""
        decltype(input.find('"', 0)) pos{0};
        while ((pos = input.find('"', pos)) != std::string::npos)
        {
            input.insert(input.begin() + pos, '"');
            pos += 2;
        }
        return "\"" + input + "\"";
    }
}

void saveCsvFile(const std::string& filename, const sol::table& csv, const sol::table& options)
{
    std::filesystem::create_directories(std::filesystem::path{filename}.parent_path());
    std::ofstream output{filename};

    if (hasField(options, "columns"))
    {
        auto const columnsTable = readField<sol::table>(options, "columns");
        auto const columnCount = columnsTable.size();
        std::vector<std::string> columns;
        for (auto i = 0U; i < columnCount; ++i)
        {
            auto const name = readField<std::string>(columnsTable, i + 1);
            columns.push_back(name);
        }
        auto const csvSize = csv.size();
        for (auto r = 0U; r < csvSize; ++r)
        {
            auto const row = readField<sol::table>(csv, r + 1);
            for (auto c = 0U; c < columnCount; ++c)
            {
                if (c > 0)
                {
                    output << ",";
                }
                if (hasField(row, columns[c]))
                {
                    output << escapeCsvString(readField<std::string>(row, columns[c]));
                }
            }
            output << "\n";
        }
    }
    else
    {
        throw std::runtime_error{"expected 'columns' option"};
    }

    output.close();
}

auto loadCsvFile(const std::string& filename, const sol::table& options, sol::this_state s)
    -> sol::table
{
    CsvReader reader{filename};
    if (hasField(options, "columns"))
    {
        auto const columns = readField<sol::table>(options, "columns");
        auto const columnCount = columns.size();
        for (auto i = 0U; i < columnCount; ++i)
        {
            auto const name = readField<std::string>(columns, i + 1);
            reader.setColumnName(i, name);
        }
    }
    sol::state_view lua{s};
    sol::table csvData = lua.create_table();
    unsigned r{0};
    while (auto maybeRow = reader.nextRow())
    {
        auto const& row = *maybeRow;
        auto rowData = lua.create_table();
        for (auto i = 0U; i < row.size(); ++i)
        {
            if (reader.hasColumnName(i))
            {
                rowData[reader.columnName(i)] = row[i];
            }
            else
            {
                rowData[i + 1] = row[i];
            }
        }
        csvData[r + 1] = rowData;
        ++r;
    }
    return csvData;
}

auto StringCollection_strings(const StringCollection& collection, sol::this_state s) -> sol::table
{
    sol::state_view lua{s};
    sol::table table = lua.create_table();
    auto const strings = collection.strings();
    for (auto i = 0U; i < strings.size(); ++i)
    {
        table[i + 1] = strings[i];
    }
    return table;
}

void Binary_append(Binary& lhs, const Binary& rhs)
{
    lhs += rhs;
}

void Binary_write(Binary& binary, size_t offset, uint8_t value)
{
    binary.writeLittle<1>(offset, value);
}

auto Binary_read(const Binary& binary, size_t offset) -> uint8_t
{
    return binary.readLittle<1, uint8_t>(offset);
}

auto openBaseLibrary(sol::this_state state) -> sol::table
{
    sol::state_view lua(state);

    auto const bytecode = lua::fuse::open("PrettyPrint.lua");
    auto const reader = [](lua_State*, void* voidBytecode, size_t* size) {
        auto const* bytecode = reinterpret_cast<std::pair<const uint8_t*, size_t>*>(voidBytecode);
        *size = bytecode->second;
        return reinterpret_cast<const char*>(bytecode->first);
    };
    auto library = lua.load(reader, (void*)&bytecode, "[PrettyPrint]");
    auto result = library();
    if (!result.valid())
    {
        throw FuseException{ "could not open library PrettyPrint" };
    }
    if (result.get_type() != sol::type::table)
    {
        throw FuseException{ "library must return a table" };
    }

    sol::table module = lua.create_table();
    module["pprint"] = result.get<sol::table>();
    module.new_enum("SIGNEDNESS", "SIGNED", Signedness::Signed, "UNSIGNED", Signedness::Unsigned);
    module.new_enum("ENDIANNESS", "LITTLE", Endianness::Little, "BIG", Endianness::Big);
    module.new_usertype<Binary>("Binary", "load", sol::factories(&loadBinary), "save", &saveBinary,
                                "new", sol::constructors<Binary(), Binary(size_t)>(), "size",
                                sol::property(&Binary::size), "append", &Binary_append, "write",
                                &Binary_write, "read", &Binary_read);
    module.new_usertype<StringCollection>("StringCollection", "insert", &StringCollection::insert,
                                          "strings", StringCollection_strings);
    module["loadcsv"] = &loadCsvFile;
    module["savecsv"] = &saveCsvFile;

    return module;
}

} // namespace fuse