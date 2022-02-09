#include <libbuild2/compiler/target.hxx>

namespace build2
{
  namespace compiler
  {
    // compiler.generated
    //
    group_view generated::
    group_members (action a) const
    {
      static_assert (sizeof (generated_pair[2]) == sizeof (const target*) * 4,
                     "member layout incompatible with array");

      if (members.empty ())
        return group_view {nullptr, 0};

      // Members discovered during anything other than perform_update are only
      // good for that operation. We also re-discover the members on each
      // update and clean not to overcomplicate the already twisted
      // compile_rule::apply() logic.
      //
      if (members_on != ctx.current_on)
      {
        if (members_action != perform_update_id ||
            a == perform_update_id ||
            a == perform_clean_id)
          return group_view {nullptr, 0};
      }

      return group_view {
        reinterpret_cast<const target* const*> (members.data ()),
        members.size () * 2};
    }

    const target_type generated::static_type
    {
      "compiler.generated",
      &mtime_target::static_type,
      &target_factory<generated>,
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      &target_search,
      true // "See through" default iteration mode.
    };
  }
}
