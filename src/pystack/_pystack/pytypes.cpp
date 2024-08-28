#include <algorithm>
#include <assert.h>
#include <cstdlib>
#include <ios>
#include <iterator>
#include <utility>

#include "logging.h"
#include "pytypes.h"
#include "structure.h"
#include "version.h"

namespace pystack {

namespace {
template<typename Range, typename Value = typename Range::value_type>
std::string
join(Range const& elements, const char* const delimiter)
{
    std::ostringstream os;
    auto b = begin(elements), e = end(elements);

    if (b == e) {
        return "";
    }

    std::copy(b, prev(e), std::ostream_iterator<Value>(os, delimiter));
    os << *prev(e);

    return os.str();
}
}  // namespace

static const std::string ELLIPSIS = "...";

template<typename T>
std::string
limitOutput(T&& arg, ssize_t max_size)
{
    if (max_size - arg.size() > 0) {
        return std::forward<T>(arg);
    }
    return ELLIPSIS;
}

std::string
formatSequence(
        const std::vector<remote_addr_t>& items,
        const std::shared_ptr<const AbstractProcessManager>& manager,
        ssize_t max_size)
{
    std::vector<std::string> elements;
    elements.reserve(items.size());
    ssize_t remaining_size = max_size;
    for (auto& item : items) {
        LOG(DEBUG) << "Constructing sequence object " << elements.size()
                   << " from addr: " << std::showbase << std::hex << item;
        std::string item_str = Object(manager, item).toString(remaining_size);
        remaining_size -= item_str.size() + 2;
        if (remaining_size < (ssize_t)(ELLIPSIS.size() + 2)) {
            elements.push_back(ELLIPSIS);
            break;
        }
        elements.push_back(item_str);
    }
    return join(elements, ", ");
}

static inline bool
containsOnlyASCII(const std::string& val)
{
    return std::all_of(val.cbegin(), val.cend(), [](auto& c) {
        return static_cast<unsigned char>(c) < 127;
    });
}

static inline std::string
normalizeBytesObjectRepresentation(const std::string& val, const std::string& prefix = "b")
{
    if (containsOnlyASCII(val)) {
        return prefix + '"' + val + '"';
    }
    return "<BINARY>";
}

TupleObject::TupleObject(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        remote_addr_t addr)
{
    d_manager = manager;

    Structure<py_tuple_v> tuple(manager, addr);
    ssize_t num_items = tuple.getField(&py_tuple_v::o_ob_size);
    if (num_items == 0) {
        LOG(DEBUG) << std::hex << std::showbase << "There are no elements in this tuple";
        return;
    }
    d_items.resize(num_items);
    manager->copyMemoryFromProcess(
            tuple.getFieldRemoteAddress(&py_tuple_v::o_ob_item),
            num_items * sizeof(PyObject*),
            d_items.data());
}

std::string
TupleObject::toString(ssize_t max_size) const
{
    const ssize_t remaining_size = max_size - 2;  // Make room for the '(' and the ')'
    return "(" + formatSequence(Items(), d_manager, remaining_size) + ")";
}

const std::vector<remote_addr_t>&
TupleObject::Items() const
{
    return d_items;
}

ListObject::ListObject(const std::shared_ptr<const AbstractProcessManager>& manager, remote_addr_t addr)
{
    d_manager = manager;

    Structure<py_list_v> list(manager, addr);
    ssize_t num_items = list.getField(&py_list_v::o_ob_size);
    if (num_items == 0) {
        LOG(DEBUG) << std::hex << std::showbase << "There are no elements in this list";
        return;
    }
    d_items.resize(num_items);
    manager->copyMemoryFromProcess(
            (remote_addr_t)list.getField(&py_list_v::o_ob_item),
            num_items * sizeof(PyObject*),
            d_items.data());
}

std::string
ListObject::toString(ssize_t max_size) const
{
    const ssize_t remaining_size = max_size - 2;  // Make room for the '[' and the ']'
    return "[" + formatSequence(Items(), d_manager, remaining_size) + "]";
}

const std::vector<remote_addr_t>&
ListObject::Items() const
{
    return d_items;
}

LongObject::LongObject(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        remote_addr_t addr,
        bool is_bool)
: d_is_bool(is_bool)
{
#ifdef ENVIRONMENT64
    constexpr unsigned int shift = 30;
#else
    constexpr unsigned int shift = 15;
#endif

    Structure<py_long_v> longobj(manager, addr);
    ssize_t size;
    bool negative;

    Py_ssize_t ob_size = longobj.getField(&py_long_v::o_ob_size);
    if (manager->versionIsAtLeast(3, 12)) {
        auto lv_tag = *reinterpret_cast<uintptr_t*>(&ob_size);
        negative = (lv_tag & 3) == 2;
        size = lv_tag >> 3;
    } else {
        negative = ob_size < 0;
        size = std::abs(ob_size);
    }

    if (size == 0) {
        d_value = 0;
        return;
    }

    /* Python's Include/longobjrep.h has this declaration:
     *      struct _longobject {
     *          PyObject_VAR_HEAD
     *          digit ob_digit[1];
     *      };
     *
     *  with this description:
     *    The absolute value of a number is equal to
     *          SUM(for i=0 through abs(ob_size)-1) ob_digit[i] * 2**(SHIFT*i)
     *    Negative numbers are represented with ob_size < 0;
     *    zero is represented by ob_size == 0.
     *
     *  where SHIFT can be either:
     *       #define PyLong_SHIFT        30
     *       #define PyLong_SHIFT        15
     */

    std::vector<digit> digits;
    digits.resize(size);
    manager->copyMemoryFromProcess(
            longobj.getFieldRemoteAddress(&py_long_v::o_ob_digit),
            sizeof(digit) * size,
            digits.data());
    for (ssize_t i = 0; i < size; ++i) {
        long long factor;
        if (__builtin_mul_overflow(digits[i], (1Lu << (ssize_t)(shift * i)), &factor)) {
            d_overflowed = true;
            return;
        }
        if (__builtin_add_overflow(d_value, factor, &d_value)) {
            d_overflowed = true;
            return;
        }
    }

    if (negative) {
        d_value = -1 * d_value;
    }
}

std::string
LongObject::toString(ssize_t max_size) const
{
    if (d_is_bool) {
        if (Value() > 0) {
            return "True";
        }
        return "False";
    }

    if (Overflowed()) {
        return "<UNRESOLVED BIG INT>";
    }

    return limitOutput(std::to_string(Value()), max_size);
}
long long int
LongObject::Value() const
{
    return d_value;
}
bool
LongObject::Overflowed() const
{
    return d_overflowed;
}

void
getDictEntries(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        Structure<py_dict_v>& dict,
        ssize_t& num_items,
        std::vector<Python3::PyDictKeyEntry>& valid_entries)
{
    assert(manager->versionIsAtLeast(3, 0));
    remote_addr_t keys_addr = dict.getField(&py_dict_v::o_ma_keys);
    ssize_t dk_size = 0;
    int dk_kind = 0;

    Structure<py_dictkeys_v> keys(manager, keys_addr);
    num_items = keys.getField(&py_dictkeys_v::o_dk_nentries);
    dk_size = keys.getField(&py_dictkeys_v::o_dk_size);

    if (manager->versionIsAtLeast(3, 11)) {
        // We're reusing the o_dk_size offset for dk_log2_size. Fix up the value.
        dk_size = 1L << dk_size;
        // Added in 3.11
        dk_kind = keys.getField(&py_dictkeys_v::o_dk_kind);
    }
    if (num_items == 0) {
        LOG(DEBUG) << std::hex << std::showbase << "There are no elements in this dict";
        return;
    }
    /*
     * The size in bytes of an indice depends on dk_size:
     *
     *   - 1 byte if dk_size <= 0xff (char*)
     *   - 2 bytes if dk_size <= 0xffff (int16_t*)
     *   - 4 bytes if dk_size <= 0xffffffff (int32_t*)
     *   - 8 bytes otherwise (int64_t*)
     */
    ssize_t offset;
    if (dk_size <= 0xFF) {
        offset = dk_size;
    } else if (dk_size <= 0xFFFF) {
        offset = 2 * dk_size;
    } else if (dk_size <= 0xFFFFFFFF) {
        offset = 4 * dk_size;
    } else {
        offset = 8 * dk_size;
    }

    offset_t dk_indices_addr = keys.getFieldRemoteAddress(&py_dictkeys_v::o_dk_indices);
    remote_addr_t entries_addr = dk_indices_addr + offset;

    std::vector<Python3::PyDictKeyEntry> raw_entries;
    raw_entries.resize(num_items);

    if (dk_kind != 0) {  // New PyDictUnicodeEntry
        std::vector<Python3_11::PyDictUnicodeEntry> unicode_entries;
        unicode_entries.resize(num_items);
        manager->copyMemoryFromProcess(
                entries_addr,
                num_items * sizeof(Python3_11::PyDictUnicodeEntry),
                unicode_entries.data());
        std::transform(
                unicode_entries.cbegin(),
                unicode_entries.cend(),
                std::back_inserter(raw_entries),
                [](auto& entry) {
                    return Python3::PyDictKeyEntry{0, entry.me_key, entry.me_value};
                });
    } else {
        manager->copyMemoryFromProcess(
                entries_addr,
                num_items * sizeof(Python3::PyDictKeyEntry),
                raw_entries.data());
    }

    // Filter out the entries that are empty
    std::copy_if(
            make_move_iterator(raw_entries.cbegin()),
            make_move_iterator(raw_entries.cend()),
            std::back_inserter(valid_entries),
            [](auto& entry) { return entry.me_key != 0; });
}

bool
DictObject::Invalid() const
{
    return d_invalid;
}

const std::vector<remote_addr_t>&
DictObject::Keys() const
{
    return d_keys;
}
const std::vector<remote_addr_t>&
DictObject::Values() const
{
    return d_values;
}

DictObject::DictObject(std::shared_ptr<const AbstractProcessManager> manager, remote_addr_t addr)
: d_manager(std::move(manager))
{
    // For now, the layout that we use here only allows us to get Python3.6+ dictionaries
    // as dictionaries before that have much more variability and are much harder to get.

    if (d_manager->versionIsAtLeast(3, 6)) {
        loadFromPython3(addr);
    } else if (d_manager->versionIsAtLeast(3, 0)) {
        d_invalid = true;
    } else {
        loadFromPython2(addr);
    }
}

void
DictObject::loadFromPython3(remote_addr_t addr)
{
    Structure<py_dict_v> dict(d_manager, addr);

    ssize_t num_items;
    std::vector<Python3::PyDictKeyEntry> valid_entries;

    getDictEntries(d_manager, dict, num_items, valid_entries);

    // Copy the keys
    d_keys.reserve(valid_entries.size());
    std::transform(
            valid_entries.cbegin(),
            valid_entries.cend(),
            std::back_inserter(d_keys),
            [](auto& entry) { return (remote_addr_t)entry.me_key; });

    /* The DictObject can be in one of two forms.
     *
     *          Either:
     *  A combined table:
     *  ma_values == NULL, dk_refcnt == 1.
     *  Values are stored in the me_value field of the PyDictKeysObject.
     *          Or:
     *  A split table:
     *  ma_values != NULL, dk_refcnt >= 1
     *  Values are stored in the ma_values array.
     *          Only string (unicode) keys are allowed.
     *          All dicts sharing same key must have same insertion order.
     */

    remote_addr_t dictvalues_addr = dict.getField(&py_dict_v::o_ma_values);
    Structure<py_dictvalues_v> dictvalues(d_manager, dictvalues_addr);

    // Get the values in one copy if we are dealing with a split-table dictionary
    if (dictvalues_addr != 0) {
        d_values.resize(num_items);
        auto values_addr = dictvalues.getFieldRemoteAddress(&py_dictvalues_v::o_values);
        d_manager->copyMemoryFromProcess(values_addr, num_items * sizeof(PyObject*), d_values.data());
    } else {
        std::transform(
                valid_entries.cbegin(),
                valid_entries.cend(),
                std::back_inserter(d_values),
                [](auto& entry) { return (remote_addr_t)entry.me_value; });
    }
}

void
DictObject::loadFromPython2(remote_addr_t addr)
{
    Python2::PyDictObject dict;
    d_manager->copyObjectFromProcess(addr, &dict);

    ssize_t num_items = dict.ma_mask + 1;
    std::vector<Python2::PyDictEntry> raw_entries;
    raw_entries.resize(num_items);

    auto entries_addr = (remote_addr_t)dict.ma_table;
    d_manager->copyMemoryFromProcess(
            entries_addr,
            num_items * sizeof(Python2::PyDictEntry),
            raw_entries.data());

    std::vector<Python2::PyDictEntry> valid_entries;
    // Filter out the entries that are empty
    std::copy_if(
            std::make_move_iterator(raw_entries.cbegin()),
            std::make_move_iterator(raw_entries.cend()),
            std::back_inserter(valid_entries),
            [](auto& entry) { return entry.me_value != 0; });

    // Copy the keys
    std::vector<remote_addr_t> keys;
    keys.reserve(valid_entries.size());
    std::transform(
            valid_entries.cbegin(),
            valid_entries.cend(),
            std::back_inserter(d_keys),
            [](auto& entry) { return (remote_addr_t)entry.me_key; });

    // Copy the values
    d_values.reserve(valid_entries.size());
    std::transform(
            valid_entries.cbegin(),
            valid_entries.cend(),
            std::back_inserter(d_values),
            [](auto& entry) { return (remote_addr_t)entry.me_value; });
}

std::string
DictObject::toString(ssize_t max_size) const
{
    if (Invalid()) {
        return "<UNRESOLVED DICT OBJECT>";
    }
    std::vector<std::string> elements;
    elements.reserve(Keys().size());

    // Make room for the "{" and the "}"
    ssize_t remaining_size = max_size - 2;

    for (size_t i = 0; i < Keys().size(); ++i) {
        const remote_addr_t key_addr = Keys()[i];
        LOG(DEBUG) << "Constructing dictionary key " << i << " from addr: " << std::showbase << std::hex
                   << key_addr;
        std::string item_str = Object(d_manager, key_addr).toString(remaining_size);
        item_str += ": ";
        const remote_addr_t val_addr = Values()[i];
        LOG(DEBUG) << "Constructing dictionary value " << i << " from addr: " << std::showbase
                   << std::hex << val_addr;
        item_str += Object(d_manager, val_addr).toString(remaining_size);
        remaining_size -= item_str.size() + 2;
        if (remaining_size < 5) {
            elements.push_back(ELLIPSIS);
            break;
        }
        elements.push_back(item_str);
    }
    return "{" + join(elements, ", ") + "}";
}

GenericObject::GenericObject(remote_addr_t addr, std::string classname)
: d_addr(addr)
, d_classname(std::move(classname))
{
}

std::string
GenericObject::toString(ssize_t max_size) const
{
    std::stringstream os;
    os << "<" << std::hex << d_classname << " at 0x" << d_addr << ">";
    std::string object_str = os.str();
    return limitOutput(object_str, max_size);
}

NoneObject::NoneObject(remote_addr_t addr)
: d_addr(addr)
{
}

std::string
NoneObject::toString([[maybe_unused]] ssize_t max_size) const
{
    return "None";
}

Object::Object(const std::shared_ptr<const AbstractProcessManager>& manager, remote_addr_t addr)
: d_addr(addr)
, d_type_addr(0)
, d_classname()
, d_flags()
, d_manager(manager)
{
    LOG(DEBUG) << std::hex << std::showbase << "Copying PyObject data from address " << addr;

    Structure<py_object_v> obj(manager, addr);
    try {
        obj.copyFromRemote();
    } catch (RemoteMemCopyError& ex) {
        LOG(WARNING) << std::hex << std::showbase << "Failed to read PyObject data from address "
                     << d_addr;
        d_classname = "invalid object";
        return;
    }

    d_type_addr = obj.getField(&py_object_v::o_ob_type);
    LOG(DEBUG) << std::hex << std::showbase << "Copying typeobject from address " << d_type_addr;
    Structure<py_type_v> cls(manager, d_type_addr);
    try {
        d_flags = cls.getField(&py_type_v::o_tp_flags);
    } catch (RemoteMemCopyError& ex) {
        LOG(WARNING) << std::hex << std::showbase << "Failed to read typeobject from address "
                     << d_type_addr;
        d_classname = "invalid object";
        return;
    }

    remote_addr_t name_addr = cls.getField(&py_type_v::o_tp_name);
    try {
        d_classname = manager->getCStringFromAddress(name_addr);
    } catch (RemoteMemCopyError& ex) {
        // If the original ELF files are not available, we can try to guess the class
        // name from other available information, specially for the types where the
        // class name is needed to categorize then.
        d_classname = guessClassName(cls);
    }
    LOG(DEBUG) << "Object class resolved to: " << d_classname;
}

bool
Object::hasFlags(unsigned long flags) const
{
    return flags & d_flags;
}

remote_addr_t
Object::typeAddr() const
{
    return d_type_addr;
}

// Helpers for making overloaded lambdas in the variant visitor in Object::toString
template<class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};

template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

std::string
Object::toString(ssize_t max_size) const
{
    // Check if we have enough room for an ellipsis and container surrounding
    // characters and if not, replace the object with an ellipsis.
    if (max_size <= 5) {
        return ELLIPSIS;
    }
    try {
        std::stringstream os;
        std::visit(
                overloaded{
                        [&](const auto& arg) { os << arg.toString(max_size); },
                        [&](const bool arg) { os << arg; },
                        [&](const long arg) { os << arg; },
                        [&](const double arg) { os << arg; },
                        [&](const std::string& arg) {
                            std::string truncated = arg;
                            if (static_cast<size_t>(max_size) < arg.size()) {
                                truncated = arg.substr(0, max_size - 3) + "...";
                            }
                            os << truncated;
                        },
                },
                toConcreteObject());

        return os.str();
    } catch (RemoteMemCopyError& ex) {
        LOG(WARNING) << std::hex << std::showbase << "Failed to create a repr for object of type "
                     << d_classname << " at address " << d_addr;

        std::stringstream os;
        os << std::hex << std::showbase << "<" << d_classname << " object at " << d_addr << ">";
        return os.str();
    }
}

long
Object::toInteger() const
{
    _PyIntObject the_int;
    d_manager->copyObjectFromProcess(d_addr, &the_int);
    return the_int.ob_ival;
}

double
Object::toFloat() const
{
    Structure<py_float_v> the_float(d_manager, d_addr);
    return the_float.getField(&py_float_v::o_ob_fval);
}

bool
Object::toBool() const
{
    if (toInteger() > 0) {
        return true;
    }
    return false;
}

Object::ObjectType
Object::objectType() const
{
    // clang-format off
    constexpr long subclass_mask =
            (Pystack_TPFLAGS_INT_SUBCLASS
             | Pystack_TPFLAGS_LONG_SUBCLASS
             | Pystack_TPFLAGS_LIST_SUBCLASS
             | Pystack_TPFLAGS_TUPLE_SUBCLASS
             | Pystack_TPFLAGS_BYTES_SUBCLASS
             | Pystack_TPFLAGS_UNICODE_SUBCLASS
             | Pystack_TPFLAGS_DICT_SUBCLASS
             | Pystack_TPFLAGS_BASE_EXC_SUBCLASS
             | Pystack_TPFLAGS_TYPE_SUBCLASS);
    // clang-format on

    const long subclass_flags = d_flags & subclass_mask;

    if (subclass_flags == Pystack_TPFLAGS_BYTES_SUBCLASS) {
        return d_manager->versionIsAtLeast(3, 0) ? ObjectType::BYTES : ObjectType::STRING;
    } else if (subclass_flags == Pystack_TPFLAGS_UNICODE_SUBCLASS) {
        return ObjectType::STRING;
    } else if (subclass_flags == Pystack_TPFLAGS_INT_SUBCLASS) {
        if (d_classname == "bool") {
            return ObjectType::INT_BOOL;
        }
        return ObjectType::INT;
    } else if (subclass_flags == Pystack_TPFLAGS_LONG_SUBCLASS) {
        if (d_classname == "bool") {
            return ObjectType::LONG_BOOL;
        }
        return ObjectType::LONG;
    } else if (subclass_flags == Pystack_TPFLAGS_TUPLE_SUBCLASS) {
        return ObjectType::TUPLE;
    } else if (subclass_flags == Pystack_TPFLAGS_LIST_SUBCLASS) {
        return ObjectType::LIST;
    } else if (subclass_flags == Pystack_TPFLAGS_DICT_SUBCLASS) {
        return ObjectType::DICT;
    } else if (d_classname == "float") {
        return ObjectType::FLOAT;
    } else if (d_classname == "NoneType") {
        return ObjectType::NONE;
    } else if (d_classname == "code") {
        return ObjectType::CODE;
    }
    return ObjectType::OTHER;
}

Object::PythonObject
Object::toConcreteObject() const
{
    try {
        switch (objectType()) {
            case Object::ObjectType::STRING:
                if (d_manager->versionIsAtLeast(3, 0)) {
                    return '"' + d_manager->getStringFromAddress(d_addr) + '"';
                }
                return normalizeBytesObjectRepresentation(d_manager->getStringFromAddress(d_addr), "");
            case Object::ObjectType::BYTES:
                return normalizeBytesObjectRepresentation(d_manager->getBytesFromAddress(d_addr));
            case Object::ObjectType::NONE:
                return NoneObject(d_addr);
            case Object::ObjectType::INT:
                return toInteger();
            case Object::ObjectType::INT_BOOL:
                return toBool();
            case Object::ObjectType::LONG:
                return LongObject(d_manager, d_addr);
            case Object::ObjectType::LONG_BOOL:
                return LongObject(d_manager, d_addr, true);
            case Object::ObjectType::FLOAT:
                return toFloat();
            case Object::ObjectType::TUPLE:
                return TupleObject(d_manager, d_addr);
            case Object::ObjectType::LIST:
                return ListObject(d_manager, d_addr);
            case Object::ObjectType::DICT:
                return DictObject(d_manager, d_addr);
            case Object::ObjectType::CODE:
            case Object::ObjectType::OTHER:
                return GenericObject(d_addr, d_classname);
        }
    } catch (InvalidRemoteObject&) {
        LOG(DEBUG) << std::hex << std::showbase << "Failed to identify object at address: " << d_addr;
    }
    return GenericObject(d_addr, d_classname);
}

std::string
Object::guessClassName(Structure<py_type_v>& type) const
{
    remote_addr_t tp_repr = type.getField(&py_type_v::o_tp_repr);
    if (tp_repr == d_manager->findSymbol("float_repr")) {
        return "float";
    }
    if (tp_repr == d_manager->findSymbol("none_repr")) {
        return "NoneType";
    }
    if (tp_repr == d_manager->findSymbol("bool_repr")) {
        return "bool";
    }
    if (tp_repr == d_manager->findSymbol("code_repr")) {
        return "PyCodeObject";
    }
    return "???";
}

}  // namespace pystack
