// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/ui_test_utils.h"
#include "gfx/point.h"
#include "gfx/rect.h"
#include "googleurl/src/gurl.h"

#if defined(OS_WIN) || defined(OS_MACOSX)
// Test succeeds locally, flaky on trybot
// http://code.google.com/p/chromium/issues/detail?id=26349
// http://code.google.com/p/chromium/issues/detail?id=45581
#define TestOnMouseOut DISABLED_TestOnMouseOut
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

namespace {

class MouseLeaveTest : public UITest {
 public:
  MouseLeaveTest() {
    dom_automation_enabled_ = true;
    show_window_ = true;
  }

  DISALLOW_COPY_AND_ASSIGN(MouseLeaveTest);
};

TEST_F(MouseLeaveTest, TestOnMouseOut) {
  GURL test_url = ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("mouseleave.html")));

  scoped_refptr<BrowserProxy> browser = automation()->GetBrowserWindow(0);
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window = browser->GetWindow();
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());

  gfx::Rect tab_view_bounds;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_CONTAINER, &tab_view_bounds,
                                    true));
  gfx::Point in_content_point(
      tab_view_bounds.x() + tab_view_bounds.width() / 2,
      tab_view_bounds.y() + 10);
  gfx::Point above_content_point(
      tab_view_bounds.x() + tab_view_bounds.width() / 2,
      tab_view_bounds.y() - 2);

  // Start by moving the point just above the content.
  ASSERT_TRUE(window->SimulateOSMouseMove(above_content_point));

  // Navigate to the test html page.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(test_url));

  const int timeout_ms = 5 * action_max_timeout_ms();

  // Wait for the onload() handler to complete so we can do the
  // next part of the test.
  ASSERT_TRUE(WaitUntilCookieValue(
      tab.get(), test_url, "__state", timeout_ms, "initial"));

  // Move the cursor to the top-center of the content, which will trigger
  // a javascript onMouseOver event.
  ASSERT_TRUE(window->SimulateOSMouseMove(in_content_point));

  // Wait on the correct intermediate value of the cookie.
  ASSERT_TRUE(WaitUntilCookieValue(
      tab.get(), test_url, "__state", timeout_ms, "initial,entered"));

  // Move the cursor above the content again, which should trigger
  // a javascript onMouseOut event.
  ASSERT_TRUE(window->SimulateOSMouseMove(above_content_point));

  // Wait on the correct final value of the cookie.
  ASSERT_TRUE(WaitUntilCookieValue(
      tab.get(), test_url, "__state", timeout_ms, "initial,entered,left"));
}

}  // namespace
