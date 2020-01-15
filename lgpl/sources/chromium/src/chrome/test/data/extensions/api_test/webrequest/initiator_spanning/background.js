// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var initiators = [];

function onBeforeRequest(details) {
  if (details.initiator && details.url.includes('title1.html'))
    initiators.push(details.initiator);
}

chrome.webRequest.onBeforeRequest.addListener(
    onBeforeRequest, {types: ['sub_frame'], urls: ['<all_urls>']});

chrome.test.sendMessage('ready');
