#include <libbuild2/compiler/init.hxx>

#include <libbuild2/diagnostics.hxx>

using namespace std;

namespace build2
{
  namespace compiler
  {
    bool
    init (scope&,
          scope&,
          const location& l,
          bool,
          bool,
          module_init_extra&)
    {
      info (l) << "module compiler initialized";
      return true;
    }

    static const module_functions mod_functions[] =
    {
      {"compiler", nullptr, init},
      {nullptr, nullptr, nullptr}
    };

    const module_functions*
    build2_compiler_load ()
    {
      return mod_functions;
    }
  }
}
