#include "dlms/profile/profile_skeleton.hpp"

#include <gtest/gtest.h>

TEST(ProfileSkeleton, Available)
{
  EXPECT_TRUE(dlms::profile::ProfileSkeletonAvailable());
}

