#include "loaders/LuaArrayFormatLoader.h"
#include "loaders/LuaIntegerFormatLoader.h"
#include "loaders/LuaRecordFormatLoader.h"
#include "loaders/LuaRelativeOffsetFormatLoader.h"
#include "loaders/LuaStringFormatLoader.h"
#include <fstream>
#include <fuse/addresses/Address.h>
#include <fuse/addresses/AddressFormat.h>
#include <fuse/binary/DataRangeTracker.h>
#include <fuse/binary/DataReader.h>
#include <fuse/binary/DataWriter.h>
#include <fuse/binary/LuaBinaryLibrary.h>
#include <fuse/binary/data/Data.h>
#include <fuse/binary/formats/ArrayFormat.h>
#include <fuse/binary/formats/DataFormat.h>
#include <fuse/binary/formats/IntegerFormat.h>
#include <fuse/binary/formats/PointerFormat.h>
#include <fuse/binary/formats/RecordFormat.h>
#include <fuse/binary/formats/StringFormat.h>
#include <fuse/binary/serialization/CsvSerialization.h>
#include <fuse/binary/serialization/LuaSerialization.h>
#include <fuse/binary/serialization/Serialization.h>
#include <fuse/lua/LuaWriter.h>
#include <sol.hpp>

namespace fuse::binary {

auto loadStringFormat(const sol::table& format, sol::this_state state)
    -> std::unique_ptr<StringFormat>
{
    LuaStringFormatLoader loader;
    if (auto maybeStringFormat = loader.load(format, state))
    {
        return std::move(*maybeStringFormat);
    }
    else
    {
        throw std::runtime_error{"could not construct StringFormat"};
    }
}

auto loadArrayFormat(const sol::table& format, sol::this_state state)
    -> std::unique_ptr<ArrayFormat>
{
    LuaArrayFormatLoader loader;
    if (auto maybeArrayFormat = loader.load(format, state))
    {
        return std::move(*maybeArrayFormat);
    }
    else
    {
        throw std::runtime_error{"could not construct ArrayFormat"};
    }
}

auto loadRecordFormat(const sol::table& format, sol::this_state state)
    -> std::unique_ptr<RecordFormat>
{
    LuaRecordFormatLoader loader;
    if (auto maybeRecordFormat = loader.load(format, state))
    {
        return std::move(*maybeRecordFormat);
    }
    else
    {
        throw std::runtime_error{"could not construct RecordFormat"};
    }
}

auto loadIntegerFormat(const sol::table& format, sol::this_state state)
    -> std::unique_ptr<IntegerFormat>
{
    LuaIntegerFormatLoader loader;
    if (auto maybeIntegerFormat = loader.load(format, state))
    {
        return std::move(*maybeIntegerFormat);
    }
    else
    {
        throw std::runtime_error{"could not construct IntegerFormat"};
    }
}

auto loadRelativeOffsetFormat(const sol::table& format, sol::this_state state)
    -> std::unique_ptr<PointerFormat>
{
    LuaRelativeOffsetFormatLoader loader;
    if (auto maybePointerFormat = loader.load(format, state))
    {
        return std::move(*maybePointerFormat);
    }
    else
    {
        throw std::runtime_error{"could not construct PointerFormat"};
    }
}

auto makeDataReader(const std::string& filename) -> std::unique_ptr<DataReader>
{
    return std::make_unique<DataReader>(filename);
}

auto makeDataWriter() -> std::unique_ptr<DataWriter>
{
    return std::make_unique<DataWriter>();
}

/*
auto getDataReaderRanges(const DataReader& reader, sol::this_state state) -> sol::table
{
    sol::state_view lua{state};
    auto const& ranges = reader.ranges();
    sol::table rangesTable = lua.create_table();
    for (auto const& range : ranges)
    {
        sol::table rangeTable = lua.create_table();
        rangeTable["address"] = range.second.address;
        rangeTable["size"] = range.second.size;
        rangesTable[range.first.toString()] = rangeTable;
    }
    return rangesTable;
}
*/

void DataReader_setAddressMap(DataReader& reader, const AddressMap* addressMap)
{
    reader.setAddressMap(addressMap->copy());
}

static void serializeData(const Data& data, const std::string& filename, sol::this_state state)
{
    std::filesystem::path path{filename};
    if (path.extension() == ".csv")
    {
        CsvSerialization serialization;
        serialization.serialize(data, path);
    }
    else if (path.extension() == ".lua")
    {
        LuaSerialization serialization{state};
        serialization.serialize(data, path);
    }
    else
    {
        throw std::runtime_error{"serialize: unsupported type"};
    }
}

static auto deserializeData(const std::string& filename, sol::this_state state)
    -> std::unique_ptr<Data>

{
    std::filesystem::path path{filename};
    if (path.extension() == ".csv")
    {
        CsvSerialization serialization;
        if (auto data = serialization.deserialize(path))
        {
            return std::move(data);
        }
        else
        {
            throw std::runtime_error{"deserialize: failed"};
        }
    }
    else if (path.extension() == ".lua")
    {
        LuaSerialization serialization{state};
        if (auto data = serialization.deserialize(path))
        {
            return std::move(data);
        }
        else
        {
            throw std::runtime_error{"deserialize: failed"};
        }
    }
    else
    {
        throw std::runtime_error{"deserialize: unsupported type"};
    }
}

static void DataFormat_encode(DataFormat& format, DataWriter& writer, const sol::object& data,
                              const std::string& path, sol::this_state state)
{
    if (auto maybePath = DataPath::fromString(path))
    {
        writer.startData(*maybePath);
        if (data.is<const Data&>())
        {
            format.encode(writer, data.as<const Data&>());
        }
        else
        {
            LuaSerialization serialization{state};
            auto deserialized = serialization.read(data);
            format.encode(writer, *deserialized);
        }
        writer.finishData();
    }
    else
    {
        throw std::runtime_error{"'" + path + "' is not a valid data path"};
    }
}

static void DataWriter_saveObjects(DataWriter& writer, const std::string& path)
{
    std::filesystem::path metaPath{path};
    std::filesystem::path binaryPath{metaPath};
    binaryPath.replace_extension(".bin");
    auto const binary = writer.assemble();
    binary.save(metaPath, binaryPath);
}

static auto TagOnlyRangeTracker_new() -> std::shared_ptr<TagOnlyRangeTracker>
{
    return std::make_shared<TagOnlyRangeTracker>();
}

static auto TagOnlyRangeTracker_save(const TagOnlyRangeTracker& tracker,
                                     const std::string& filename)
{
    std::ofstream output{filename};
    if (output.good())
    {
        LuaWriter writer;
        tracker.serialize(writer);
        output << "return " << writer.lua();
    }
    else
    {
        throw std::runtime_error{"Could not open file for writing: " + filename};
    }
}

auto openBinaryLibrary(sol::this_state state) -> sol::table
{
    sol::state_view lua{state};
    auto module = lua.create_table();

    module.new_enum("DATA_TYPE", "ARRAY", DataType::Array, "BINARY", DataType::Binary, "CUSTOM",
                    DataType::Custom, "IMAGE", DataType::Image, "INTEGER", DataType::Integer,
                    "NULL", DataType::Null, "RECORD", DataType::Record, "REFERENCE",
                    DataType::Reference, "STRING", DataType::String);

    module.new_usertype<DataRangeTracker>("DataRangeTracker");
    module.new_usertype<TagOnlyRangeTracker>(
        "TagOnlyRangeTracker", "new", sol::factories(&TagOnlyRangeTracker_new), "save_ranges",
        &TagOnlyRangeTracker_save, sol::base_classes, sol::bases<DataRangeTracker>());
    module.new_usertype<Data>("Data", "deserialize", sol::factories(&deserializeData), "serialize",
                              &serializeData);
    module.new_usertype<DataReader>(
        "DataReader", "new", sol::factories(&makeDataReader), "set_offset", &DataReader::setOffset,
        "set_tracker", &DataReader::setTracker,
        /*"decoded_ranges", &getDataReaderRanges,*/ "set_address_map", &DataReader_setAddressMap);
    module.new_usertype<DataWriter>("DataWriter", "new", sol::factories(&makeDataWriter),
                                    "save_objects", &DataWriter_saveObjects);

    module.new_usertype<DataFormat>("DataFormat", "decode", &DataFormat::decode, "encode",
                                    &DataFormat_encode);
    module.new_usertype<StringFormat>("StringFormat", sol::call_constructor, &loadStringFormat,
                                      sol::base_classes, sol::bases<DataFormat>());
    module.new_usertype<ArrayFormat>("ArrayFormat", sol::call_constructor, &loadArrayFormat,
                                     sol::base_classes, sol::bases<DataFormat>());
    module.new_usertype<RecordFormat>("RecordFormat", sol::call_constructor, &loadRecordFormat,
                                      sol::base_classes, sol::bases<DataFormat>());
    module.new_usertype<PointerFormat>("PointerFormat", sol::call_constructor,
                                       &loadRelativeOffsetFormat, sol::base_classes,
                                       sol::bases<DataFormat>());
    module.new_usertype<IntegerFormat>("IntegerFormat", sol::call_constructor, &loadIntegerFormat,
                                       sol::base_classes, sol::bases<DataFormat>());

    return module;
}

} // namespace fuse::binary