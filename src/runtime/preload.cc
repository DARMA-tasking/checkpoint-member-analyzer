/*
//@HEADER
// *****************************************************************************
//
//                                  preload.cc
//                           DARMA Toolkit v. 1.0.0
//                       DARMA/Serialization Sanitizer
//
// Copyright 2019 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact darma@sandia.gov
//
// *****************************************************************************
//@HEADER
*/

#include "common.h"
#include "preload.h"
#include "sanitize_rt.h"

#include <stdlib.h>

namespace checkpoint { namespace sanitizer {

#define SANITIZER_HOOK(name) \
  struct name##_Hook :                                                  \
    PreloaderHook<decltype(&::name), name##_Hook>                       \
  {                                                                     \
    static constexpr char const* ident = #name;                         \
  } name

SANITIZER_HOOK(MPI_Init);
SANITIZER_HOOK(checkpoint_sanitizer_rt);
SANITIZER_HOOK(checkpoint_sanitizer_enabled);

}} /* end namespace checkpoint::sanitizer */

extern "C" {

int MPI_Init(int *argc, char ***argv) {
  checkpoint::sanitizer::MPI_Init.init();

  debug_sanitizer("Intercepted MPI_Init\n");

#if 0
  std::vector<std::string> new_args;

  for (int i = 0; i < *argc; i++) {
    fmt::print("ARG {}: {}\n", i, (*argv)[i]);
    if (std::string{(*argv)[i]} == std::string("--vt_sanitize_file")) {
      checkpoint::sanitizer::output_as_file = true;
    } else {
      new_args.push_back((*argv)[i]);
    }
  }
#endif

  char* to_file = getenv("VT_SANITIZE_OUTPUT_FILE");

  if (to_file != nullptr) {
    auto str = std::string{to_file};
    if (str == "1" or str == "ON" or str == "on" or str == "true" or str == "TRUE") {
      checkpoint::sanitizer::output_as_file = true;
    }
  }

  if (checkpoint::sanitizer::MPI_Init) {
#if 0
    int num_argc = static_cast<int>(new_args.size());
    std::vector<char*> new_argv;

    for (auto&& e : new_args) {
      new_argv.push_back(const_cast<char*>(e.c_str()));
    }

    char** raw_argv = &new_argv[0];

    return checkpoint::sanitizer::MPI_Init(&num_argc, &raw_argv);
#endif

    return checkpoint::sanitizer::MPI_Init(argc, argv);
  } else {
    fmt::print(stderr, "Failed for forward symbol\n");
    return 1;
  }
}

checkpoint::sanitizer::Runtime* checkpoint_sanitizer_rt() {
  debug_sanitizer("Intercepted checkpoint_sanitizer_rt\n");

  static std::unique_ptr<checkpoint::sanitizer::Sanitizer> active_rt = nullptr;

  if (active_rt == nullptr) {
    active_rt = std::make_unique<checkpoint::sanitizer::Sanitizer>();
  }
  return active_rt.get();
}

bool checkpoint_sanitizer_enabled() {
  debug_sanitizer("Intercepted checkpoint_sanitizer_enabled\n");
  return true;
}

} /* end extern "C" */
