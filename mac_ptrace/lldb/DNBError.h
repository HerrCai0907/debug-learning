//===-- DNBError.h ----------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  Created by Greg Clayton on 6/26/07.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_TOOLS_DEBUGSERVER_SOURCE_DNBERROR_H
#define LLDB_TOOLS_DEBUGSERVER_SOURCE_DNBERROR_H

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mach/mach.h>
#include <ostream>
#include <string>

class DNBError {
public:
  typedef uint32_t ValueType;
  enum FlavorType { Generic = 0, MachKernel = 1, POSIX = 2 };

  explicit DNBError(ValueType err = 0, FlavorType flavor = Generic) : m_err(err), m_flavor(flavor) {}

  const char *AsString() const;
  DNBError Clear() {
    m_err = 0;
    m_flavor = Generic;
    m_str.clear();
    return *this;
  }
  ValueType Status() const { return m_err; }
  FlavorType Flavor() const { return m_flavor; }

  ValueType operator=(kern_return_t err) {
    m_err = err;
    m_flavor = MachKernel;
    m_str.clear();
    return m_err;
  }

  void SetError(kern_return_t err) {
    m_err = err;
    m_flavor = MachKernel;
    m_str.clear();
  }

  DNBError SetErrorToErrno() {
    m_err = errno;
    m_flavor = POSIX;
    m_str.clear();
    return *this;
  }

  void SetError(ValueType err, FlavorType flavor) {
    m_err = err;
    m_flavor = flavor;
    m_str.clear();
  }

  // Generic errors can set their own string values
  DNBError SetErrorString(const char *err_str) {
    if (err_str && err_str[0]) {
      m_str = err_str;
    } else {
      m_str.clear();
    }
    return *this;
  }
  bool Success() const { return m_err == 0; }
  bool Fail() const { return m_err != 0; }

  void check() {
    if (Fail()) {
      std::cerr << *this << "\n";
      std::exit(-1);
    }
  }

  friend std::ostream &operator<<(std::ostream &os, DNBError &self) { return os << self.AsString(); }

protected:
  ValueType m_err;
  FlavorType m_flavor;
  mutable std::string m_str;
};

#endif // LLDB_TOOLS_DEBUGSERVER_SOURCE_DNBERROR_H
