// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BASE_API_H_
#define BASE_BASE_API_H_
#pragma once

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(BASE_IMPLEMENTATION)
#define BASE_API __declspec(dllexport)
#else
#define BASE_API __declspec(dllimport)
#endif  // defined(BASE_IMPLEMENTATION)

#else  // defined(WIN32)
#define BASE_API __attribute__((visibility("default")))
#endif

#else  // defined(COMPONENT_BUILD)
#define BASE_API
#endif

#endif  // BASE_BASE_API_H_
