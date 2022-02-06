#pragma once

#include <libbuild2/types.hxx>
#include <libbuild2/utility.hxx>

#include <libbuild2/module.hxx>

#include <libbuild2/compiler/export.hxx>

namespace build2
{
  namespace compiler
  {
    extern "C" LIBBUILD2_COMPILER_SYMEXPORT const module_functions*
    build2_compiler_load ();
  }
}
