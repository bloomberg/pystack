#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "elf_common.h"
#include "mem.h"
#include "native_frame.h"

namespace pystack {

class UnwinderError : public std::exception
{
  public:
    explicit UnwinderError(std::string error)
    : d_error(std::move(error)){};

    [[nodiscard]] const char* what() const noexcept override
    {
        return d_error.c_str();
    }

  private:
    std::string d_error;
};

class Frame
{
  public:
    Frame(Dwarf_Addr pc, bool isActivation, std::optional<Dwarf_Word> stackPointer)
    : pc(pc)
    , isActivation(isActivation)
    , stackPointer(stackPointer)
    {
    }
    Dwarf_Addr pc;
    bool isActivation;
    std::optional<Dwarf_Word> stackPointer;
};

class ModuleCuDieRanges
{
  public:
    // Methods
    Dwarf_Die* moduleAddrDie(Dwfl_Module* mod, Dwarf_Addr addr, Dwarf_Addr* bias);

  private:
    // Classes
    class CuDieRanges
    {
      public:
        // Classes
        struct CuDieRange
        {
            Dwarf_Die* cuDie;
            Dwarf_Addr bias;
            Dwarf_Addr low;
            Dwarf_Addr high;

            inline bool contains(Dwarf_Addr addr) const
            {
                return low <= addr && addr < high;
            }
        };

        // Methods
        explicit CuDieRanges(Dwfl_Module* mod = nullptr);

        Dwarf_Die* findDie(Dwarf_Addr addr, Dwarf_Addr* bias) const;

        // Data members
        std::vector<CuDieRange> d_ranges;
    };

    // Data members
    std::unordered_map<Dwfl_Module*, CuDieRanges> d_die_range_maps;
};

class AbstractUnwinder
{
  public:
    // Methods
    virtual remote_addr_t
    getAddressforSymbol(const std::string& symbol, const std::string& modulename) const;
    virtual std::vector<NativeFrame> unwindThread(pid_t tid) const = 0;

    // Static methods
    static std::string demangleSymbol(const std::string&);

    virtual ~AbstractUnwinder() = default;

  protected:
    // Methods
    virtual struct Dwfl* Dwfl() const = 0;
    std::vector<NativeFrame> gatherFrames(const std::vector<Frame>& frames) const;

  private:
    // Enums
    enum class StatusCode {
        SUCCESS,
        ERROR,
    };
    // Aliases
    using Scopes = std::shared_ptr<Dwarf_Die>;
    using ScopesInfo = std::pair<int, Scopes>;

    // Methods
    Dwarf_Die* dwarfModuleAddrDie(Dwarf_Addr pc_adjusted, Dwfl_Module* mod, Dwarf_Addr* bias) const;

    std::pair<int, Scopes> dwarfGetScopes(Dwarf_Die* cudie, Dwarf_Addr pc_adjusted) const;
    std::pair<int, Scopes> dwarfGetScopesDie(Dwarf_Die* die) const;

    const char* getNonInlineSymbolName(Dwfl_Module* mod, Dwarf_Addr pc) const;

    StatusCode gatherInlineFrames(
            std::vector<NativeFrame>& native_frames,
            const std::string& noninline_symname,
            Dwarf_Addr pc,
            Dwarf_Addr pc_corrected,
            Dwarf_Die* cudie,
            const char* mod_name) const;

    // Data members
    mutable ModuleCuDieRanges d_range_maps_cache;
    mutable std::unordered_map<Dwarf_Addr, ScopesInfo> d_dwarf_getscopes_cache;
    mutable std::unordered_map<void*, ScopesInfo> d_dwarf_getscopes_die_cache;
    mutable std::unordered_map<Dwarf_Addr, const char*> d_symbol_by_pc_cache;
};

class Unwinder : public AbstractUnwinder
{
  public:
    // Constructors
    explicit Unwinder(std::shared_ptr<ProcessAnalyzer> analyzer);

    // Methods
    virtual struct Dwfl* Dwfl() const override;
    std::vector<NativeFrame> unwindThread(pid_t tid) const override;

  private:
    // Data members
    std::shared_ptr<ProcessAnalyzer> d_analyzer;
};

class CoreFileUnwinder : public AbstractUnwinder
{
  public:
    // Constructors
    explicit CoreFileUnwinder(std::shared_ptr<CoreFileAnalyzer> analyzer);

    // Methods
    virtual struct Dwfl* Dwfl() const override;
    std::vector<int> getCoreTids() const;
    std::vector<NativeFrame> unwindThread(pid_t tid) const override;

  private:
    // Data members
    std::shared_ptr<CoreFileAnalyzer> d_analyzer;
};
}  // namespace pystack
