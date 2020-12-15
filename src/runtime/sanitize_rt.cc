/*
//@HEADER
// *****************************************************************************
//
//                                sanitize_rt.cc
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
#include "sanitize_rt.h"

#include <cassert>
#include <unistd.h>
#include <cxxabi.h>

namespace checkpoint { namespace sanitizer {

bool output_as_file = false;
bool output_colorize = true;

void Sanitizer::checkMember(void* addr, std::string name, std::string tinfo) {
  assert(stack_.size() > 0 && "Must have valid live stack");
  debug_sanitizer(
    "check: {}, name={}, tinfo={}: size={}\n",
    static_cast<void const*>(addr), name, tinfo, stack_.size()
  );
  stack_.back().checkElm(addr, name, tinfo);
}

void Sanitizer::skipMember(void* addr, std::string name, std::string tinfo) {
  assert(stack_.size() > 0 && "Must have valid live stack");
  debug_sanitizer(
    "skip: {}, name={}, tinfo={}: size={}\n",
    static_cast<void const*>(addr), name, tinfo, stack_.size()
  );
  stack_.back().ignoreElm(addr, name, tinfo);
}

void Sanitizer::isSerialized(void* addr, std::size_t num, std::string tinfo) {
  // At a top-level, nothing to do!
  if (stack_.size() == 0) {
    return;
  }
  debug_sanitizer(
    "isSerialized: {}, num={}. tinfo={}: size={}\n",
    static_cast<void const*>(addr), num, tinfo, stack_.size()
  );
  stack_.back().isSerialized(addr, tinfo);
}

void Sanitizer::push(std::string tinfo) {
  stack_.push_back(StackRecord{tinfo});
  debug_sanitizer("push: tinfo={} : level={}\n", tinfo, stack_.size());
}

void Sanitizer::pop(std::string tinfo) {
  debug_sanitizer("pop: tinfo={} : level={}\n", tinfo, stack_.size());

  assert(stack_.back().getName() == tinfo && "Unmatched pop of stack");

  // before we pop check the validity of this stack frame.
  checkValidityFrame();

  stack_.pop_back();
}

void Sanitizer::checkValidityFrame() {
  debug_sanitizer("checkValidityFrame: size={}\n", stack_.size());

  assert(stack_.size() > 0 && "Must have a valid live stack");
  auto& e = stack_.back();

  auto const& checked = e.getCheck();
  auto const& ignored = e.getIgnored();
  auto const& is_serialized = e.getIsSerial();

  for (auto&& elm : checked) {
    // Skip check for elements that are explicitly skipped/ignored by the user
    auto ignore_iter = ignored.find(elm);
    if (ignore_iter != ignored.end()) {
      continue;
    }

    auto ser_iter = is_serialized.find(elm);
    if (ser_iter == is_serialized.end()) {
      debug_sanitizer(
        "**missing: name={}, tinfo={}, addr={} : level={}\n",
        elm.name, elm.tinfo, elm.addr, stack_.size()
      );

      // we are missing a element in the serializer
      std::vector<std::string> stack;
      for (auto i = stack_.rbegin(); i != stack_.rend(); i++) {
        stack.push_back(i->getName());
      }

      auto missing_iter = missing_.find(elm.name);
      if (missing_iter == missing_.end()) {
        missing_.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(elm.name),
          std::forward_as_tuple(
            std::make_unique<MissingInfo>(elm.name, elm.tinfo, stack)
          )
        );
      } else {
        missing_iter->second->addStack(stack);
      }
    }
  }
}

static std::string demangle(char const* name) {
  int status = 0;

  std::unique_ptr<char, void(*)(void*)> res {
    abi::__cxa_demangle(name, NULL, NULL, &status),
    std::free
  };

  return status == 0 ? res.get() : name;
}

inline bool colorizeOutput() {
  return output_colorize;
}

inline std::string green()    { return colorizeOutput() ? "\033[32m"   : ""; }
inline std::string bold()     { return colorizeOutput() ? "\033[1m"    : ""; }
inline std::string magenta()  { return colorizeOutput() ? "\033[95m"   : ""; }
inline std::string red()      { return colorizeOutput() ? "\033[31m"   : ""; }
inline std::string bred()     { return colorizeOutput() ? "\033[31;1m" : ""; }
inline std::string reset()    { return colorizeOutput() ? "\033[00m"   : ""; }
inline std::string bd_green() { return colorizeOutput() ? "\033[32;1m" : ""; }
inline std::string it_green() { return colorizeOutput() ? "\033[32;3m" : ""; }
inline std::string un_green() { return colorizeOutput() ? "\033[32;4m" : ""; }
inline std::string byellow()  { return colorizeOutput() ? "\033[33;1m" : ""; }
inline std::string yellow()   { return colorizeOutput() ? "\033[33m"   : ""; }
inline std::string blue()     { return colorizeOutput() ? "\033[34m"   : ""; }

inline std::string emph(std::string str) {
  return magenta() + str + reset();
}
inline std::string reg(std::string str) {
  return green() + str + reset();
}
inline std::string vtPre() {
  return bd_green() + std::string("vt:sanitizer") + reset() + ": ";
}
inline std::string proc(pid_t pid)  {
  return blue() + "" + std::to_string(pid) + ":" + reset();
}

template <typename... Args>
static void outputPidLines(
  FILE* fd, pid_t pid, std::string const& line, Args&&... args
) {
  auto s = fmt::format(proc(pid) + vtPre() + line, std::forward<Args>(args)...);
  fmt::print(fd, s);
}

void Sanitizer::printSummary() {
  std::vector<MissingInfo*> m;
  for (auto&& e : missing_) {
    m.push_back(e.second.get());
  }
  std::sort(m.begin(), m.end(), [](MissingInfo* m1, MissingInfo* m2) -> bool {
    return m1->getInstances() > m2->getInstances();
  });
  FILE* fd = stdout;
  std::string pid_str = "";
  auto pid = getpid();
  if (output_as_file) {
    pid_str = fmt::format("{}.sanitize.out", pid);
    auto nfd = fopen(pid_str.c_str(), "a");
    if (nfd) {
      fd = nfd;
    } else {
      perror("Error opening file: ");
      fmt::print(stderr, "Failed to open file {}\n", pid);
    }
  }

  outputPidLines(
    fd, pid,
    yellow() +  "===========================================\n" + reset()
  );
  outputPidLines(
    fd, pid,
    yellow() + "===== " +
    green() + "Serialization Sanitizer Output" +
    yellow() + " ======\n" +
    reset()
  );
  outputPidLines(
    fd, pid,
    yellow() + "===========================================\n" + reset()
  );

  for (auto&& e : m) {
    auto const& name = e->getName();
    auto const& tinfo = e->getTinfo();
    auto const& stacks = e->getStacks();
    auto const& insts = e->getInstances();

    outputPidLines(fd, pid, "---- Found missing serialized member ----\n");
    outputPidLines(fd, pid, "-----------------------------------------\n");
    outputPidLines(
      fd, pid, "---- {}{}{} -- {}{} instances{} ----\n",
      bred(), name, reset(), bold(), insts, reset()
    );
    outputPidLines(
      fd, pid, "---- {}type: {}{} ---- \n", magenta(), tinfo, reset()
    );
    for (std::size_t i = 0; i < stacks.size(); i++) {
      auto const& cs = stacks.at(i);
      auto const& stack = cs.getStack();
      auto const& sinsts = cs.getInstances();
      outputPidLines(
        fd, pid, "---- {}stack {}{}, {}{} instances{} \n",
        bd_green(), i, reset(), bold(), sinsts, reset()
      );
      for (std::size_t j = 0; j < stack.size(); j++) {
        outputPidLines(
          fd, pid, "\t {}{}{}\n", red(), demangle(stack.at(j).c_str()), reset()
        );
      }
    };
    outputPidLines(fd, pid, "----------------------------------------\n");
  }

  if (fd != stdout) {
    fmt::print("Sanitizer: wrote output to {}\n", pid_str);
    fclose(fd);
  }
}

}} /* end namespace checkpoint::sanitizer */
