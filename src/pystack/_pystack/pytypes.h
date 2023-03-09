#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

#include "mem.h"
#include "process.h"
#include "pycompat.h"

namespace pystack {

std::string
addrToString(const std::shared_ptr<const AbstractProcessManager>& manager, remote_addr_t addr);

class TupleObject
{
  public:
    // Constructors
    TupleObject(const std::shared_ptr<const AbstractProcessManager>& manager, remote_addr_t addr);

    // Getters
    const std::vector<remote_addr_t>& Items() const;

    // Methods
    std::string toString(ssize_t max_size) const;

  private:
    // Data members
    std::vector<remote_addr_t> d_items{};
    std::shared_ptr<const AbstractProcessManager> d_manager{nullptr};
};

class ListObject
{
  public:
    // Constructors
    ListObject(const std::shared_ptr<const AbstractProcessManager>& manager, remote_addr_t addr);

    // Getters
    const std::vector<remote_addr_t>& Items() const;

    // Methods
    std::string toString(ssize_t max_size) const;

  private:
    // Data members
    std::vector<remote_addr_t> d_items{};
    std::shared_ptr<const AbstractProcessManager> d_manager{nullptr};
};

class DictObject
{
  public:
    // Constructors
    DictObject(std::shared_ptr<const AbstractProcessManager> manager, remote_addr_t addr);
    // Methods
    std::string toString(ssize_t max_size) const;

    // Getters
    bool Invalid() const;
    const std::vector<remote_addr_t>& Keys() const;
    const std::vector<remote_addr_t>& Values() const;

  private:
    // Data members
    bool d_invalid{false};
    std::shared_ptr<const AbstractProcessManager> d_manager{nullptr};
    std::vector<remote_addr_t> d_keys{};
    std::vector<remote_addr_t> d_values{};

    // Methods
    void loadFromPython2(remote_addr_t addr);
    void loadFromPython3(remote_addr_t addr);
};

class LongObject
{
  public:
    // Constructors
    LongObject(
            const std::shared_ptr<const AbstractProcessManager>& manager,
            remote_addr_t addr,
            bool is_bool = false);

    // Methods
    std::string toString(ssize_t max_size) const;

  private:
    // Data members
    long long d_value{0};
    bool d_overflowed{false};
    bool d_is_bool{false};

  public:
    long long int Value() const;
    bool Overflowed() const;
};

class GenericObject
{
  public:
    GenericObject(remote_addr_t addr, std::string classname);

    // Methods
    std::string toString(ssize_t max_size) const;

  private:
    remote_addr_t d_addr;
    std::string d_classname;
};

class NoneObject
{
  public:
    explicit NoneObject(remote_addr_t addr);

    // Methods
    std::string toString(ssize_t max_size) const;

  private:
    remote_addr_t d_addr;
};

class Object
{
  public:
    // Alias
    using PythonObject = std::variant<
            std::string,
            bool,
            long,
            double,
            TupleObject,
            ListObject,
            DictObject,
            LongObject,
            NoneObject,
            GenericObject>;

    // Enums
    enum class ObjectType {
        BYTES,
        STRING,
        NONE,
        INT_BOOL,
        LONG_BOOL,
        INT,
        LONG,
        FLOAT,
        TUPLE,
        LIST,
        DICT,
        OTHER,
    };

    static constexpr int MAX_LOCAL_STR_SIZE = 80;

    // Constructors
    Object(const std::shared_ptr<const AbstractProcessManager>& manager, remote_addr_t addr);

    // Methods
    ObjectType objectType() const;
    PythonObject toConcreteObject() const;
    bool hasFlags(unsigned long flags) const;
    std::string toString(ssize_t max_size = MAX_LOCAL_STR_SIZE) const;

  private:
    // Data members
    remote_addr_t d_addr;
    std::string d_classname{};
    unsigned long d_flags{};
    std::shared_ptr<const AbstractProcessManager> d_manager{nullptr};

    // Methods
    bool toBool() const;
    long toInteger() const;
    double toFloat() const;
    std::string guessClassName(PyTypeObject& type) const;
};

}  // namespace pystack
