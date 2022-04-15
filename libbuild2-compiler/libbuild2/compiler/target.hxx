#pragma once

#include <libbuild2/types.hxx>
#include <libbuild2/utility.hxx>

#include <libbuild2/target.hxx>

#include <libbuild2/cxx/target.hxx>

namespace build2
{
  namespace compiler
  {
    // std::vector<generated_pair>.data() is standard layout compatible with
    // group_view's const target*[2*N].
    //
    struct generated_pair
    {
      const cxx::hxx* h = nullptr;
      const cxx::cxx* c = nullptr;
    };

    class generated: public mtime_target
    {
    public:
      vector<generated_pair> members;
      action members_action; // Action on which members were resolved.
      size_t members_on = 0; // Operation number on which members were resolved.

      void
      reset_members (action a)
      {
        members.clear ();
        members_action  = a;
        members_on = ctx.current_on;
      }

      generated (context& c, dir_path d, dir_path o, string n)
          : mtime_target (c, move (d), move (o), move (n))
      {
        dynamic_type = &static_type;
      }

      virtual group_view
      group_members (action) const override;

    public:
      static const target_type static_type;
    };
  }
}
