# Serialization Sanitizer

This project uses a combination of compile-time code generation with a Clang
frontend tooling pass and a runtime sanitizer to discover missing members during
serialization of classes.

The Clang frontend pass traverses all classes included in a C++ file that have a
`serialize` method. The compile-time pass then generates code (either by
modifying the source code or tacking on partial specializations) that traverse
all members discovered by the compile-time traversal.

At runtime, if sanitizer partial specializations exist for a class, the
sanitizer will check the memory addresses of the members traversed by the user
against the ones the compiler found. After a program runs, the sanitizer will
output the class members that were not traversed by the serializer as they may
indicate an error.

## Building

- Get `docker` and `docker-compose`. Then, to build `cd` into the repository
directory after cloning from git.

- **Optional**: by default, the sanitizer will build with `clang-5.0` based on
  the variables set in `.env`. This can be modified by exporting an environment
  variable `COMPILER` before running `docker-compose` commands.

```shell
export COMPILER=clang-7
```

- Build the latest VT develop image with the selected Clang compiler. If that
  image as been uploaded it may be alternatively fetched from Docker Hub.

```shell
docker-compose build vt-base
```

- **Optional**: fetch the sanitizer base image (Clang frontend tooling and
  associated dependencies) if the clang compiler version exists. If it doesn't
  exist for your preferred version, build it yourself (next step).

```shell
docker-compose pull sanitizer-base
```

- Bring images up on `docker-compose` (will build base sanitizer image if
  needed):

```shell
docker-compose up
```

- Compile and build the latest sanitizer code:

```shell
docker-compose run sanitizer-base
```

- Launch the VT container interactively with the sanitizer built:

```shell
docker-compose run vt-base
```

## Running

Once the `sanitizer` binary is built, it can be run on a C++ file with a JSON
compilation database so it knows how to compile the C++ file (includes, flags,
etc).

```shell
# /serialization-sanitizer/sanitizer /build/vt/compile-comands.json /vt/examples/hello_world.cc
```

A compilation database can be generated for a cmake project by defining a cmake
variable for that project. Set `-DCMAKE_EXPORT_COMPILE_COMMANDS=1` while
configuring the project that is going to be sanitized. When that variable is
set, the JSON compilation database will be generated in the cmake build
directory with the file name `compile_commands.json`.

### Command template
```shell
./sanitizer -p <json-compilation-database> <cc-file> -extra-arg=-std=c++1y

```
