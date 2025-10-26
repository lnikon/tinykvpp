#include <gtest/gtest.h>

#include "posix_wrapper/open_flag.h"

namespace pw = posix_wrapper;

// Mapping between enum and POSIX open flags
// kReadOnly = O_RDONLY,
// kWriteOnly = O_WRONLY,
// kReadWrite = O_RDWR,
// kAppend = O_APPEND,
// kCreate = O_CREAT,
// kTruncate = O_TRUNC,
// kExclusive = O_EXCL,
// kNonBlock = O_NONBLOCK,
// kSync = O_SYNC,
// kDirect = O_DIRECT,

TEST(PosixWrapperTest, OpenFlagToNative)
{
    EXPECT_EQ(pw::to_native(pw::open_flag_k::kReadOnly), O_RDONLY);
    EXPECT_EQ(pw::to_native(pw::open_flag_k::kWriteOnly), O_WRONLY);
    EXPECT_EQ(pw::to_native(pw::open_flag_k::kReadWrite), O_RDWR);
    EXPECT_EQ(pw::to_native(pw::open_flag_k::kAppend), O_APPEND);
    EXPECT_EQ(pw::to_native(pw::open_flag_k::kCreate), O_CREAT);
    EXPECT_EQ(pw::to_native(pw::open_flag_k::kTruncate), O_TRUNC);
    EXPECT_EQ(pw::to_native(pw::open_flag_k::kExclusive), O_EXCL);
    EXPECT_EQ(pw::to_native(pw::open_flag_k::kNonBlock), O_NONBLOCK);
    EXPECT_EQ(pw::to_native(pw::open_flag_k::kSync), O_SYNC);
    EXPECT_EQ(pw::to_native(pw::open_flag_k::kDirect), O_DIRECT);
}
