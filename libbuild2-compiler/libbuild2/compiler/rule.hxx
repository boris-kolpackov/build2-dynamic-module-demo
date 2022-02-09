#pragma once

#include <libbuild2/types.hxx>
#include <libbuild2/utility.hxx>

#include <libbuild2/rule.hxx>
#include <libbuild2/dyndep.hxx>

namespace build2
{
  namespace compiler
  {
    // Cached data shared between rules and the module.
    //
    struct data
    {
      const exe& ctgt; // Compiler target.
      string     csum; // Compiler checksum.
    };

    class compile_rule: public simple_rule, virtual data, dyndep_rule
    {
    public:
      compile_rule (data&& d)
          : data (move (d)), rule_id ("compiler.compile 1") {}

      virtual bool
      match (action, target&, const string&) const override;

      virtual recipe
      apply (action, target&) const override;

      target_state
      perform_update (action, const target&,
                      timestamp,
                      const path&,
                      const string&) const;

      target_state
      perform_clean (action, const target&) const;

    private:
      const char* rule_id;
    };
  }
}
