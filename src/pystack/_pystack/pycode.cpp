#include <cassert>
#include <stdexcept>
#include <vector>

#include "logging.h"
#include "mem.h"
#include "process.h"
#include "pycode.h"
#include "pycompat.h"
#include "pytypes.h"
#include "version.h"

static const int NO_LINE_NUMBER = -0x80;

namespace pystack {

typedef enum _PyCodeLocationInfoKind {
    PY_CODE_LOCATION_INFO_SHORT0 = 0,
    PY_CODE_LOCATION_INFO_ONE_LINE0 = 10,
    PY_CODE_LOCATION_INFO_ONE_LINE1 = 11,
    PY_CODE_LOCATION_INFO_ONE_LINE2 = 12,

    PY_CODE_LOCATION_INFO_NO_COLUMNS = 13,
    PY_CODE_LOCATION_INFO_LONG = 14,
    PY_CODE_LOCATION_INFO_NONE = 15
} _PyCodeLocationInfoKind;

static bool
parse_linetable(const uintptr_t addrq, const std::string& linetable, int firstlineno, LocationInfo* info)
{
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(linetable.c_str());
    uint64_t addr = 0;
    info->lineno = firstlineno;

    auto scan_varint = [&]() {
        unsigned int read = *ptr++;
        unsigned int val = read & 63;
        unsigned int shift = 0;
        while (read & 64) {
            read = *ptr++;
            shift += 6;
            val |= (read & 63) << shift;
        }
        return val;
    };

    auto scan_signed_varint = [&]() {
        unsigned int uval = scan_varint();
        int sval = uval >> 1;
        int sign = (uval & 1) ? -1 : 1;
        return sign * sval;
    };

    while (*ptr != '\0') {
        uint8_t first_byte = *(ptr++);
        uint8_t code = (first_byte >> 3) & 15;
        size_t length = (first_byte & 7) + 1;
        uintptr_t end_addr = addr + length;
        switch (code) {
            case PY_CODE_LOCATION_INFO_NONE: {
                break;
            }
            case PY_CODE_LOCATION_INFO_LONG: {
                int line_delta = scan_signed_varint();
                info->lineno += line_delta;
                info->end_lineno = info->lineno + scan_varint();
                info->column = scan_varint() - 1;
                info->end_column = scan_varint() - 1;
                break;
            }
            case PY_CODE_LOCATION_INFO_NO_COLUMNS: {
                int line_delta = scan_signed_varint();
                info->lineno += line_delta;
                info->column = info->end_column = -1;
                break;
            }
            case PY_CODE_LOCATION_INFO_ONE_LINE0:
            case PY_CODE_LOCATION_INFO_ONE_LINE1:
            case PY_CODE_LOCATION_INFO_ONE_LINE2: {
                int line_delta = code - 10;
                info->lineno += line_delta;
                info->end_lineno = info->lineno;
                info->column = *(ptr++);
                info->end_column = *(ptr++);
                break;
            }
            default: {
                uint8_t second_byte = *(ptr++);
                assert((second_byte & 128) == 0);
                info->column = code << 3 | (second_byte >> 4);
                info->end_column = info->column + (second_byte & 15);
                break;
            }
        }
        if (addr <= addrq && end_addr > addrq) {
            return true;
        }
        addr = end_addr;
    }
    return false;
}

static LocationInfo
getLocationInfo(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        remote_addr_t code_addr,
        PyCodeObject& code,
        uintptr_t last_instruction_index)
{
    int code_lineno = manager->versionedCodeField<unsigned int, &py_code_v::o_firstlineno>(code);
    remote_addr_t lnotab_addr = *(remote_addr_t*)((char*)&code + manager->offsets().py_code.o_lnotab);
    LOG(DEBUG) << std::hex << std::showbase << "Copying lnotab data from address " << lnotab_addr;
    std::string lnotab = manager->getBytesFromAddress(lnotab_addr);

    assert(manager->majorVersion() > 3 || manager->minorVersion() >= 11 || lnotab.size() % 2 == 0);
    std::string::size_type last_executed_instruction = last_instruction_index;

    LocationInfo location_info = LocationInfo{0, 0, 0, 0};

    // Check out https://github.com/python/cpython/blob/main/Objects/lnotab_notes.txt for the format of
    // the lnotab table in different versions of the interpreter.
    if (manager->majorVersion() > 3 || (manager->majorVersion() == 3 && manager->minorVersion() >= 11)) {
        uintptr_t code_adaptive =
                code_addr + manager->versionedCodeOffset<&py_code_v::o_code_adaptive>();
        ptrdiff_t addrq =
                (reinterpret_cast<uint16_t*>(last_instruction_index)
                 - reinterpret_cast<uint16_t*>(code_adaptive));
        LocationInfo posinfo;
        bool ret = parse_linetable(addrq, lnotab, code_lineno, &posinfo);
        if (ret) {
            location_info.lineno = posinfo.lineno;
            location_info.end_lineno = posinfo.end_lineno;
            location_info.column = posinfo.column;
            location_info.end_column = posinfo.end_column;
        }
    } else if (
            manager->majorVersion() > 3
            || (manager->majorVersion() == 3 && manager->minorVersion() == 10))
    {
        // Word-code is two bytes, so the actual limit in the table 2 * the instruction index
        last_executed_instruction <<= 1;
        for (std::string::size_type i = 0, current_instruction = 0; i < lnotab.size();) {
            unsigned char start_delta = lnotab[i++];
            signed char line_delta = lnotab[i++];
            current_instruction += start_delta;
            code_lineno += (line_delta == NO_LINE_NUMBER) ? 0 : line_delta;
            if (current_instruction > last_executed_instruction) {
                break;
            }
        }
        location_info.lineno = code_lineno;
        location_info.end_lineno = code_lineno;
    } else {
        for (std::string::size_type i = 0, bc = 0; i < lnotab.size();
             code_lineno += static_cast<int8_t>(lnotab[i++]))
        {
            bc += lnotab[i++];
            if (bc > last_executed_instruction) {
                break;
            }
        }
        location_info.lineno = code_lineno;
        location_info.end_lineno = code_lineno;
    }
    return location_info;
}

CodeObject::CodeObject(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        remote_addr_t addr,
        uintptr_t lasti)
{
    PyCodeObject code;
    LOG(DEBUG) << std::hex << std::showbase << "Copying code struct from address " << addr;
    manager->copyMemoryFromProcess(addr, manager->offsets().py_code.size, &code);

    remote_addr_t filename_addr =
            *(remote_addr_t*)((char*)&code + manager->offsets().py_code.o_filename);
    LOG(DEBUG) << std::hex << std::showbase << "Copying filename Python string from address "
               << filename_addr;
    d_filename = manager->getStringFromAddress(filename_addr);
    LOG(DEBUG) << "Code object filename: " << d_filename;

    remote_addr_t name_addr = *(remote_addr_t*)((char*)&code + manager->offsets().py_code.o_name);
    LOG(DEBUG) << std::hex << std::showbase << "Copying code name Python string from address "
               << name_addr;
    d_scope = manager->getStringFromAddress(name_addr);
    LOG(DEBUG) << "Code object scope: " << d_filename;

    LOG(DEBUG) << "Obtaining location info location";
    d_location_info = getLocationInfo(manager, addr, code, lasti);
    LOG(DEBUG) << "Code object location info: line_range=(" << d_location_info.lineno << ", "
               << d_location_info.end_lineno << ") column_range=(" << d_location_info.column << ", "
               << d_location_info.end_column << ")";

    d_narguments = manager->versionedCodeField<unsigned int, &py_code_v::o_argcount>(code);
    LOG(DEBUG) << "Code object n arguments: " << d_narguments;

    LOG(DEBUG) << "Copying variable names";
    remote_addr_t varnames_addr =
            manager->versionedCodeField<remote_addr_t, &py_code_v::o_varnames>(code);
    TupleObject varnames(manager, varnames_addr);
    std::transform(
            varnames.Items().cbegin(),
            varnames.Items().cend(),
            std::back_inserter(d_varnames),
            [&](auto& addr) {
                const std::string varname = manager->getStringFromAddress(addr);
                LOG(DEBUG) << "Variable name found: '" << varname << "'";
                return varname;
            });
}

std::string
CodeObject::Filename() const
{
    return d_filename;
}

std::string
CodeObject::Scope() const
{
    return d_scope;
}

const LocationInfo&
CodeObject::Location() const
{
    return d_location_info;
}
int
CodeObject::NArguments() const
{
    return d_narguments;
}
const std::vector<std::string>&
CodeObject::Varnames() const
{
    return d_varnames;
}
}  // namespace pystack
