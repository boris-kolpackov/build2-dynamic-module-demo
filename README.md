# build2-dynamic-module-demo

This project contains a bunch of (hypothetical) source code generation tools
and a build system module that demonstrate handling of dynamic target groups
in `build2` where the number and the names of group members (source files
in this case) cannot be predicted.

Specifically, it contains the `exe-generator` and `exe-compiler` tools, the
`libbuild2-compiler` build system modules, and the `exe-consumer` test
project. These should be initialized as described in
[`libbuild2-hello`](https://github.com/build2/libbuild2-hello/) (tools in the
`host` configuration, module -- in `build2`, and test -- in `target`).

The expected tools invocation sequence is as follows:

1. First we must call `generator` in order to obtain a cookie.

   A previously-obtained cookie is valid as long as the generator hasn't
   changed.

2. Then we call `compiler` with the `--list-source-files` option passing
   it the cookie. The result is a list of header/source files (written to
   stdout) that will be generated for this cookie. The number and names of
   the files depend on the cookie.

   A previously-obtained list for the same cookie is valid as long as the
   compiler hasn't changed.

3. Finally, we call `compiler` with the `--output-dir <dir>` option
   passing it the cookie. The result is the set of generated files written
   to `<dir>`.

   A previously-generated set of files for the same cookie is valid as long as
   the compiler hasn't changed.

All these steps and associated change tracking is handled by the rule in the
`libbuild2-compiler` module. The dynamic set of the generated source files is
represented with the `compiler.generated{}` target group. See
`exe-consumer/buildfile` for usage examples.
