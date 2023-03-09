#pragma once
#define _PYSTACK_PY_CODE_H

#include <memory>
#include <string>
#include <vector>

#include "mem.h"
#include "process.h"
#include "version.h"

namespace pystack {

struct LocationInfo
{
    int lineno;
    int end_lineno;
    int column;
    int end_column;
};

class CodeObject
{
  public:
    // Constructors
    CodeObject(
            const std::shared_ptr<const AbstractProcessManager>& manager,
            remote_addr_t addr,
            uintptr_t lastli);

    // Getters
    std::string Filename() const;
    std::string Scope() const;
    const LocationInfo& Location() const;
    int NArguments() const;
    const std::vector<std::string>& Varnames() const;

  private:
    // Data members
    std::string d_filename;
    std::string d_scope;
    LocationInfo d_location_info;
    int d_narguments;

  private:
    std::vector<std::string> d_varnames;
};
}  // namespace pystack
