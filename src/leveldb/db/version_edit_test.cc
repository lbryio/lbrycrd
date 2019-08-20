// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/version_edit.h"
#include "util/testharness.h"

namespace leveldb {

static void TestEncodeDecode(
    const VersionEdit& edit,
    bool format2=false) {
  std::string encoded, encoded2;
  edit.EncodeTo(&encoded,format2);
  VersionEdit parsed;
  Status s = parsed.DecodeFrom(encoded);
  ASSERT_TRUE(s.ok()) << s.ToString();
  parsed.EncodeTo(&encoded2,format2);
  ASSERT_EQ(encoded, encoded2);

  if (parsed.HasF1Files() || parsed.HasF2Files())
  {
      ASSERT_EQ(parsed.HasF1Files(), !format2);
      ASSERT_EQ(parsed.HasF2Files(), format2);
  }   // if
}

class VersionEditTest { };

TEST(VersionEditTest, EncodeDecode) {
  static const uint64_t kBig = 1ull << 50;

  VersionEdit edit;
  for (int i = 0; i < 4; i++) {
    TestEncodeDecode(edit);
    edit.AddFile2(3, kBig + 300 + i, kBig + 400 + i,
                  InternalKey("foo", 0, kBig + 500 + i, kTypeValue),
                  InternalKey("zoo", 0, kBig + 600 + i, kTypeDeletion),
                  0,0,0);
    edit.DeleteFile(4, kBig + 700 + i);
    edit.SetCompactPointer(i, InternalKey("x", 0, kBig + 900 + i, kTypeValue));
  }

  edit.SetComparatorName("foo");
  edit.SetLogNumber(kBig + 100);
  edit.SetNextFile(kBig + 200);
  edit.SetLastSequence(kBig + 1000);
  TestEncodeDecode(edit);
}

TEST(VersionEditTest, EncodeDecodeExpiry) {
  static const uint64_t kBig = 1ull << 25;

  VersionEdit edit;
  for (int i = 0; i < 4; i++) {
    TestEncodeDecode(edit, false); // only testing for s.ok()
    edit.AddFile2(3, kBig + 300 + i, kBig + 400 + i,
                 InternalKey("foo", 700+i, kBig + 500 + i, kTypeValueExplicitExpiry),
                 InternalKey("zoo", 800+i, kBig + 600 + i, kTypeDeletion),
                 10203040,
                 123456789,
                 987654321);
    edit.DeleteFile(4, kBig + 700 + i);
    edit.SetCompactPointer(i, InternalKey("x", 0, kBig + 900 + i, kTypeValue));
  }

  edit.SetComparatorName("foo");
  edit.SetLogNumber(kBig + 100);
  edit.SetNextFile(kBig + 200);
  edit.SetLastSequence(kBig + 1000);
  TestEncodeDecode(edit, true);
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::RunAllTests();
}
