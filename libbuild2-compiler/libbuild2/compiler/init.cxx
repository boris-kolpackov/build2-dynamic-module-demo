#include <libbuild2/compiler/init.hxx>

#include <libbuild2/file.hxx>
#include <libbuild2/diagnostics.hxx>

#include <libbuild2/compiler/module.hxx>
#include <libbuild2/compiler/target.hxx>

using namespace std;

namespace build2
{
  namespace compiler
  {
    bool
    init (scope& rs,
          scope& bs,
          const location& loc,
          bool,
          bool,
          module_init_extra& extra)
    {
      tracer trace ("compiler::init");
      l5 ([&]{trace << "for " << rs;});

      // Let's only support root loading to keep things simple (which means
      // there can only be one).
      //
      if (rs != bs)
        fail (loc) << "compiler module must be loaded in project root";

      // Make sure the cxx module has been loaded since we need its targets
      // types (?xx{}).
      //
      if (!cast_false<bool> (rs["cxx.loaded"]))
        fail (loc) << "cxx module must be loaded before compiler";

      // Import the compiler target.
      //
      // @@ TODO: add metadata to exe-compiler and use checksum for more
      //    precise change tracking.
      //
      import_result<exe> ir (
        import_direct<exe> (
          rs,
          // exe-compiler%exe{compiler}
          name ("exe-compiler", dir_path (), "exe", "compiler"),
          true      /* phase2 */,
          false     /* optional */,
          false     /* metadata */,
          loc,
          "module load"));

      auto& m (extra.set_module (new module (data {*ir.target, "" /*csum*/})));

      // Register target types.
      //
      rs.insert_target_type<generated> ();

      // Register rules.
      //
      // Other rules may need to have the group members resolved/linked
      // up (general pattern: groups should resolve on *(update)).
      //
      rs.insert_rule<generated> (0, update_id, "compiler.compile", m);
      rs.insert_rule<generated> (perform_id, clean_id, "compiler.compile", m);

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
