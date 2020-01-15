// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var menuId = 'my_id'

chrome.contextMenus.onClicked.addListener(function(info, tab) {
  chrome.test.assertEq(menuId, info.menuItemId);
  chrome.test.sendMessage('onclick fired');
});

chrome.contextMenus.create(
    {title: 'Extension Item 1', id: menuId},
    function() {
      if (!chrome.runtime.lastError) {
        chrome.test.sendMessage('created item');
      }
    }
);
