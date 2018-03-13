// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.onCreated.addListener(function(tab) {
  chrome.test.sendMessage(tab.url == 'about:blank' ?
      'hello-newtab' : 'WRONG_NEWTAB');
});
