// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the NPAPI test

#ifndef WEBKIT_PLUGINS_NPAPI_TEST_NPAPI_CONSTANTS_H_
#define WEBKIT_PLUGINS_NPAPI_TEST_NPAPI_CONSTANTS_H_

namespace NPAPIClient {

// The name of the cookie which will be used to communicate between
// the plugin and the test harness.
extern const char kTestCompleteCookie[];

// The cookie value which will be sent to the client upon successful
// test.
extern const char kTestCompleteSuccess[];

}  // namespace NPAPIClient

#endif  // WEBKIT_PLUGINS_NPAPI_TEST_NPAPI_CONSTANTS_H_
