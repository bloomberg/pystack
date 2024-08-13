#pragma once

#include <array>
#include <vector>

#include "process.h"

namespace pystack {

template<typename OffsetsStruct>
class Structure
{
  public:
    // Constructors
    Structure(std::shared_ptr<const AbstractProcessManager> manager, remote_addr_t addr);
    Structure(const Structure&) = delete;
    Structure& operator=(const Structure&) = delete;

    // Methods
    void copyFromRemote();

    template<typename FieldPointer>
    remote_addr_t getFieldRemoteAddress(FieldPointer OffsetsStruct::*field) const;

    template<typename FieldPointer>
    const typename FieldPointer::Type& getField(FieldPointer OffsetsStruct::*field);

  private:
    // Data members
    std::shared_ptr<const AbstractProcessManager> d_manager;
    remote_addr_t d_addr;
    ssize_t d_size;
    std::array<char, 512> d_footprintbuf;
    std::vector<char> d_heapbuf;
    char* d_buf;
};

template<typename OffsetsStruct>
inline Structure<OffsetsStruct>::Structure(
        std::shared_ptr<const AbstractProcessManager> manager,
        remote_addr_t addr)
: d_manager(manager)
, d_addr(addr)
, d_size(d_manager->offsets().get<OffsetsStruct>().size)
, d_buf{}
{
}

template<typename OffsetsStruct>
inline void
Structure<OffsetsStruct>::copyFromRemote()
{
    if (d_buf) {
        return;  // already copied
    }

    if (d_size < 512) {
        d_buf = &d_footprintbuf[0];
    } else {
        d_heapbuf.resize(d_size);
        d_buf = &d_heapbuf[0];
    }
    d_manager->copyMemoryFromProcess(d_addr, d_size, d_buf);
}

template<typename OffsetsStruct>
template<typename FieldPointer>
inline remote_addr_t
Structure<OffsetsStruct>::getFieldRemoteAddress(FieldPointer OffsetsStruct::*field) const
{
    offset_t offset = (d_manager->offsets().get<OffsetsStruct>().*field).offset;
    return d_addr + offset;
}

template<typename OffsetsStruct>
template<typename FieldPointer>
inline const typename FieldPointer::Type&
Structure<OffsetsStruct>::getField(FieldPointer OffsetsStruct::*field)
{
    copyFromRemote();
    offset_t offset = (d_manager->offsets().get<OffsetsStruct>().*field).offset;
    if (d_size < 0 || (size_t)d_size < sizeof(typename FieldPointer::Type)
        || d_size - sizeof(typename FieldPointer::Type) < offset)
    {
        abort();
    }
    auto address = d_buf + offset;
    return *reinterpret_cast<const typename FieldPointer::Type*>(address);
}

}  // namespace pystack
