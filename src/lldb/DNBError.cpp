//===-- DNBError.cpp --------------------------------------------*- C++ -*-===//
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

#include "DNBError.h"

#ifdef WITH_SPRINGBOARD
#include <SpringBoardServices/SpringBoardServer.h>
#endif

const char *DNBError::AsString() const {
  if (Success()) return "success";
  if (m_str.empty()) {
    const char *s = NULL;
    switch (m_flavor) {
    case MachKernel: s = ::mach_error_string(m_err); break;
    case POSIX: s = ::strerror(m_err); break;
    default: break;
    }
    if (s) m_str.assign(s);
  }
  if (m_str.empty()) return "unknown";
  return m_str.c_str();
}
