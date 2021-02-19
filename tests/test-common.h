/*
//@HEADER
// *****************************************************************************
//
//                                test-common.h
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

#if !defined INCLUDED_SANITIZER_TEST_COMMON_H
#define INCLUDED_SANITIZER_TEST_COMMON_H

#include <vector>
#include <string>
#include <memory>

namespace checkpoint { namespace serializers {

std::vector<void*> addr;
std::vector<void*> checked;

struct Sanitizer {
  template <typename Arg, typename... Args>
  void check(Arg& m, Args&&...) {
    checked.push_back(reinterpret_cast<void*>(&m));
  }
};

struct Serializer {
  // necessary for InlineGenerator
  // template <typename Arg, typename... Args>
  // void check(Arg& m, Args&&...) { }
};

template <typename SerializerT, typename T>
void operator|(SerializerT& s, T& t) {
  addr.push_back(reinterpret_cast<void*>(&t));
}

}} /* end namespace checkpoint::serializers */

template <typename T, typename... Args>
int testClass(std::string const& name, Args&&... args) {
  using checkpoint::serializers::Serializer;
  using checkpoint::serializers::Sanitizer;
  using checkpoint::serializers::addr;
  using checkpoint::serializers::checked;

  auto t = std::make_unique<T>(std::forward<Args>(args)...);

  // invoke regular serializer
  Serializer s;
  t->serialize(s);

  // invoke sanitizer overload
  Sanitizer c;
  t->serialize(c);

  int success = 1;

  if (addr.size() != checked.size()) {
    fprintf(
      stderr,
      "Failure %s: sanitized members do not match: %zu != %zu\n",
      name.c_str(), addr.size(), checked.size()
    );
    success = 0;
  }

  for (std::size_t i = 0; i < addr.size(); i++) {
    if (checked.size() > i and addr.at(i) != checked.at(i)) {
      fprintf(stderr, "Failure %s: memory address does not match\n", name.c_str());
      success = 0;
    }
  }

  addr.clear();
  checked.clear();

  if (success) {
    printf("Success %s: test passes!\n", name.c_str());
    return 0;
  } else {
    return 1;
  }
}

#endif /*INCLUDED_SANITIZER_TEST_COMMON_H*/
