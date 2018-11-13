#include <diagnostics/Contracts.h>
#include <fuse/binary/DataReader.h>
#include <fuse/binary/StringData.h>
#include <fuse/binary/StringFormat.h>

namespace fuse::binary {

void StringFormat::setEncoding(text::TextEncoding* encoding)
{
    m_encoding = encoding;
}

auto StringFormat::doDecode(DataReader& reader) -> std::unique_ptr<Data>
{
    Expects(m_encoding);
    try
    {
        auto [newOffset, string] = m_encoding->decode(reader.binary(), reader.offset());
        auto data = std::make_unique<StringData>();
        data->setValue(string);
        reader.annotateRange(reader.offset(), newOffset - reader.offset());
        reader.setOffset(newOffset);
        return std::move(data);
    }
    catch (...)
    {
        return {};
    }
}

auto StringFormat::copy() const -> std::unique_ptr<DataFormat>
{
    auto data = std::make_unique<StringFormat>();
    data->m_encoding = m_encoding;
    return std::move(data);
}

} // namespace fuse::binary