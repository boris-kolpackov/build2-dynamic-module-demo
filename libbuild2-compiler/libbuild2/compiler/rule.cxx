#include <libbuild2/compiler/rule.hxx>

#include <type_traits>

#include <libbuild2/depdb.hxx>
#include <libbuild2/scope.hxx>
#include <libbuild2/target.hxx>
#include <libbuild2/algorithm.hxx>
#include <libbuild2/filesystem.hxx>
#include <libbuild2/diagnostics.hxx>

#include <libbuild2/compiler/target.hxx>

namespace build2
{
  namespace compiler
  {
    bool compile_rule::
    match (action a, target& t) const
    {
      tracer trace ("compiler::compile_rule::match");

      // See if we have the generator executable as source.
      //
      for (prerequisite_member p: group_prerequisite_members (a, t))
      {
        if (include (a, t, p) != include_type::normal) // Excluded/ad hoc.
          continue;

        if (p.is_a<exe> ())
          return true;
      }

      // Note: if you wish, you can make it a hard error (it's unlikely
      //       any other rule could possibly match this target).
      //
      l4 ([&]{trace << "no generator executable for target " << t;});
      return false;
    }

    recipe compile_rule::
    apply (action a, target& xt) const
    {
      tracer trace ("compiler::compile_rule::apply");

      context& ctx (xt.ctx);

      // Handle matching group members.
      //
      if (const target* g = xt.group)
      {
        assert (g->is_a<generated> ());
        match_sync (a, *g);
        return group_recipe; // Execute the group's recipe.
      }

      auto& g (xt.as<generated> ());
      const scope& bs (g.base_scope ());

      // Inject dependency on the output directory.
      //
      const fsdir* dir (inject_fsdir (a, g));

      // Match prerequisites.
      //
      match_prerequisite_members (a, g);

      // For update inject dependency on the CLI compiler target.
      //
      if (a == perform_update_id)
        inject (a, g, ctgt);

      // Determine the group members and derive their file names.
      //
      auto inject_member_pair = [a, &g, &bs] (string h, string c)
      {
        path hp (move (h));
        path cp (move (c));

        // Expect these to be simple names.
        //
        assert (hp.simple() && cp.simple ());

        g.members.push_back (
          generated_pair {
            &inject_group_member<cxx::hxx> (a, bs, g, g.dir / hp).first,
            &inject_group_member<cxx::cxx> (a, bs, g, g.dir / cp).first});
      };

      if (a == perform_update_id)
      {
        // Make sure the output directory exists.
        //
        if (dir != nullptr)
          fsdir_rule::perform_update_direct (a, g);

        // See generated::group_members().
        //
        g.reset_members (a);

        // Normally we would use the name of the first member to derive the
        // depdb file name. For group like this, however, this is not
        // possible. So we use the group name with the target type name as
        // the second-level extension.
        //
        depdb dd (g.dir / (g.name + ".generated.d"));

        // First should come the rule name/version.
        //
        if (dd.expect (rule_id_) != nullptr)
          l4 ([&]{trace << "rule mismatch forcing update of " << g;});

        // Then the compiler checksum.
        //
        if (dd.expect (csum) != nullptr)
          l4 ([&]{trace << "compiler mismatch forcing update of " << g;});

        // If any of the above checks resulted in a mismatch then do
        // unconditional update.
        //
        bool u (dd.writing ());
        timestamp mt (u ? timestamp_nonexistent : dd.mtime);

        // Update prerequisite targets (normally just the generator).
        //
        const exe* gtgt (nullptr);
        for (const target* pt: g.prerequisite_targets[a])
        {
          if (pt == nullptr || pt == dir)
            continue;

          u = update (trace, a, *pt, u ? timestamp_unknown : mt) || u;

          if (gtgt == nullptr)
            gtgt = pt->is_a<exe> ();
        }

        // @@ How would we extract metadata from generator (i.e., to get
        //    its checksum)? We are running it to get a cookie, might as
        //    well run to the metadata. I guess need to factor the relevant
        //    implementation details in file.cxx into a convenient API.

        // If nothing so far triggered update, try read the cookie and the
        // list of generated files from depdb falling back to update if
        // anything goes wrong (e.g., depdb is corrupt).
        //
        string cookie;
        size_t skip (0);

        while (!u) // Breakout loop.
        {
          string* l;
          auto read = [&dd, &u, &skip, &l] () -> bool
          {
            l = dd.read ();
            if (l != nullptr)
              skip++;
            else
              u = true;

            return !u;
          };

          // Read the cookie.
          //
          if (!read ())
            break;

          cookie = move (*l);

          // Read the files. We should always end with a blank line.
          //
          for (;;)
          {
            if (!read ())
              break;

            if (l->empty ())
              break; // Done, nothing changed.

            // See below for the format of this line.
            //
            size_t p (l->find ('/'));
            assert (p != string::npos);

            inject_member_pair (string (*l, 0, p), string (*l, p + 1));
          }

          break;
        }

        if (u)
        {
          // Run the generator to get a "cookie".
          //
          if (skip == 0)
          {
            const process_path& pp (gtgt->process_path ());
            cstrings args {pp.recall_string (), nullptr};

            if (verb >= 2)
              print_process (args);
            else if (verb)
            {
              // Note: probably would not be shown at level 1 in a real rule.
              //
              print_diag ("generator", path_name ("-") /* stdout */);
            }

            cookie = run<string> (
              ctx,
              pp, args,
              1 /* finish_verbosity */,
              [] (string& line, bool) {return move (line);});

            dd.write (cookie);
          }
          else
            skip--;

          // Run the compiler with the cookie to get the list of source files.
          //
          vector<string> files;
          {
            const process_path& pp (ctgt.process_path ());
            cstrings args {
              pp.recall_string (),
              "--list-source-files",
              cookie.c_str (),
              nullptr};

            if (verb >= 2)
              print_process (args);
            else if (verb)
            {
              // Note: probably would not be shown at level 1 in a real rule.
              //
              print_diag ("compiler --list-source-files",
                          cookie,
                          path_name ("-") /* stdout */);
            }

            files = run<vector<string>> (
              ctx,
              pp, args,
              1 /* finish_verbosity */,
              [r = vector<string> ()] (string& line, bool last) mutable
              {
                r.push_back (move (line));
                return last ? move (r) : vector<string> ();
              });
          }

          // Enter source files as targets, make them our members.
          //
          assert (files.size () != 0 && files.size () % 2 == 0);

          for (auto i (files.begin ()); i != files.end (); i += 2)
          {
            if (skip == 0)
            {
              string& h (*i);
              string& c (*(i + 1));

              // It is simpler to save the pair as a single line (we don't
              // have to deal with between-lines failures, etc). Since these
              // are simple names, let's use the directory separator as a
              // separator.
              //
              dd.write (h + '/' + c);

              inject_member_pair (move (h), move (c));
            }
            else
              skip--;
          }

          // Add the terminating blank line.
          //
          dd.expect ("");
        }
        else
        {
          // Load the mtime of the first header and check for an interrupted
          // update (depdb is older than the output). We also use this mtime
          // as group mtime in perform_update().
          //
          mt = mtime (g.members.back ().h->path ());
          u = dd.mtime > mt;
        }

        dd.close (false /* mtime_check */);

        return [this,
                mt = u ? timestamp_nonexistent : mt,
                dd = move (dd.path),
                cookie = move (cookie)] (action a, const target& t)
        {
          return perform_update (a, t, mt, dd, cookie);
        };
      }
      else if (a == perform_clean_id)
      {
        // Cleaning a dynamically-discovered group like this is not easy.
        //
        // To do it precisely would require running both the generator and the
        // compiler (like we do above) which in turn would require updating
        // them, which feels like a non-starter for multiple reasons (e.g.,
        // they could be being cleaned together with this group).
        //
        // This leaves various forms of approximation:
        //
        // 1. If the generated source file names can be traced back as
        //    belonging to a specific group, then we could just remove all
        //    those that match. For example, if all the file names belonging
        //    to the group have the group as a prefix/suffix (and the user was
        //    advised not to create source files with such a prefix/suffix).
        //
        // 2. Alternatively, we could place all the generated files belonging
        //    to the group into a dedicated subdirectory and then remove it
        //    wholesale.
        //
        // 3. Finally, we could rely on depdb and the list of generated files
        //    it should contain. The two drawbacks of this approach is that if
        //    the depdb file is removed, we loose the ability to clean the
        //    generated files. Also we may need to deal with different
        //    versions of depdb. Overall, however, this is the most general
        //    solution that does not place any assumptions on the generated
        //    files. So we will go with that.
        //
        // Note also that only removing the relevant filesystem entries is not
        // enough: we actually have to populate the group with members since
        // this information could be used to clean derived targets (for
        // example, object files).
        //
        // Note also that while we may have members already populated (e.g.,
        // from a preceding update in the same batch), they are not usable
        // unless we re-match all the members. So let's just re-enter them
        // from scratch.
        //
        g.reset_members (a);

        depdb dd (g.dir / (g.name + ".generated.d"), true /* read_only */);

        while (dd.reading ()) // Breakout loop.
        {
          string* l;
          auto read = [&dd, &l] () -> bool
          {
            return (l = dd.read ()) != nullptr;
          };

          if (!read ()) // Rule id.
            break;

          if (*l != rule_id_)
            fail << "unable to clean target " << g << " with old depdb";

          if (!read ()) // Compiler checksum.
            break;

          if (!read ()) // Cookie.
            break;

          // Read the files. We should always end with a blank line.
          //
          for (;;)
          {
            if (!read ())
              break;

            if (l->empty ())
              break; // Done.

            size_t p (l->find ('/'));
            assert (p != string::npos);

            inject_member_pair (string (*l, 0, p), string (*l, p + 1));
          }

          break;
        }

        // What should we return if we couldn't find any members (e.g., depdb
        // does not exist or is corrupt)? One option would be to return an
        // empty group. But this only works well if the group can normally by
        // empty and therefore everyone is prepared to handle this case.
        // Otherwise, we stand a good chance of expected rules no longer
        // matching our dependency declarations. For example, if we have just
        // `libue{main}: compiler.generated{main}`, then there will be no rule
        // to match it if the group turns out to be empty (since there will be
        // no C++ prerequisites and no basis for the cxx rule to match).
        //
        // A better option is to return a "representative sample" with a few
        // dummy members (or, if your group naturally always has some members
        // that can be derived just from the group and/or prerequisite names,
        // e.g., an "entry point" for the user to include or some such -- then
        // those would serve even better). One thing we must make sure is that
        // these dummy members should not possibly clash with any other source
        // files. We use the same naming approach as for the depdb file.
        //
        if (g.members.empty ())
          inject_member_pair (g.name + ".generated.hpp",
                              g.name + ".generated.cpp");

        return [] (action a, const target& t)
        {
          return perform_clean_group_extra (
            a, t.as<mtime_target> (), {".generated.d"});
        };
      }
      else // Configure/dist update.
      {
        // We cannot return an accurate group membership for configure/dist
        // update because that would require running tools and that would in
        // turn require updating them. So we return a "representative sample"
        // with a few dummy members, similar to (and for the same reason as
        // in) clean.
        //
        // @@ While unlikely, what if the user wants the generated files to be
        //    distributed?
        //
        if (g.group_members (a).members == nullptr)
        {
          g.reset_members (a);

          inject_member_pair (g.name + ".generated.hpp",
                              g.name + ".generated.cpp");
        }

        return noop_recipe;
      }
    }

    target_state compile_rule::
    perform_update (action a,
                    const target& xt,
                    timestamp mt,
                    const path& dd,
                    const string& cookie) const
    {
      tracer trace ("compiler::compile_rule::perform_update");

      auto& g (xt.as<generated> ());
      context& ctx (g.ctx);

      // While all our prerequisites are already up-to-date, we still have to
      // execute them to keep the dependency counts straight.
      //
      target_state ps (execute_prerequisites (a, g));

      if (mt != timestamp_nonexistent)
      {
        g.mtime (mt);
        return ps;
      }

      const process_path& pp (ctgt.process_path ());
      cstrings args {
        pp.recall_string (),
        "--output-dir", g.dir.string ().c_str (),
        cookie.c_str (),
        nullptr};

      if (verb >= 2)
        print_process (args);
      else if (verb)
        print_diag ("compiler", cookie, g);

      timestamp start;
      if (!ctx.dry_run)
      {
        start = depdb::mtime_check ()
          ? system_clock::now ()
          : timestamp_unknown;

        run (ctx, pp, args, 1 /* finish_verbosity */);
      }

      timestamp now (system_clock::now ());

      if (!ctx.dry_run)
        depdb::check_mtime (start, dd, g.members.back ().h->path (), now);

      g.mtime (now);
      return target_state::changed;
    }
  }
}
