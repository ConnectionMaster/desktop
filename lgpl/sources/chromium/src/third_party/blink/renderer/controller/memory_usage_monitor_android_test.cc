// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_usage_monitor_android.h"

#include <unistd.h>

#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(MemoryUsageMonitorAndroidTest, CalculateProcessFootprint) {
  MemoryUsageMonitorAndroid monitor;

  const char kStatusFile[] =
      "First:  1\n Second: 2 kB\nVmSwap: 10 kB \n Third: 10 kB\n Last: 8";
  const char kStatmFile[] = "100 40 25 0 0";
  uint64_t expected_swap_kb = 10;
  uint64_t expected_private_footprint_kb =
      (40 - 25) * getpagesize() / 1024 + expected_swap_kb;
  uint64_t expected_vm_size_kb = 100 * getpagesize() / 1024;

  base::FilePath statm_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&statm_path));
  EXPECT_EQ(static_cast<int>(sizeof(kStatmFile)),
            base::WriteFile(statm_path, kStatmFile, sizeof(kStatmFile)));
  base::File statm_file(statm_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::FilePath status_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&status_path));
  EXPECT_EQ(static_cast<int>(sizeof(kStatusFile)),
            base::WriteFile(status_path, kStatusFile, sizeof(kStatusFile)));
  base::File status_file(status_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);

  monitor.ReplaceFileDescriptorsForTesting(std::move(statm_file),
                                           std::move(status_file));

  MemoryUsage usage = monitor.GetCurrentMemoryUsage();
  EXPECT_EQ(expected_private_footprint_kb,
            static_cast<uint64_t>(usage.private_footprint_bytes / 1024));
  EXPECT_EQ(expected_swap_kb, static_cast<uint64_t>(usage.swap_bytes / 1024));
  EXPECT_EQ(expected_vm_size_kb,
            static_cast<uint64_t>(usage.vm_size_bytes / 1024));
}

}  // namespace blink
