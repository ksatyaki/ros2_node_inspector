#include <gtest/gtest.h>

#include "liveness_probe.hpp"

using rni::RateWindow;

TEST(RateWindow, UnknownUntilWindowElapses)
{
  RateWindow w(5.0);
  w.start(100.0);
  EXPECT_FALSE(w.known(102.0));   // 2 s in
  EXPECT_FALSE(w.known(104.9));
  EXPECT_TRUE(w.known(105.0));    // full window elapsed
  EXPECT_TRUE(w.known(110.0));
}

TEST(RateWindow, HzCountsArrivalsInTrailingWindow)
{
  RateWindow w(5.0);
  w.start(0.0);
  // 5 arrivals over the window => 1 Hz.
  for (double t = 1.0; t <= 5.0; t += 1.0) {
    w.record(t);
  }
  EXPECT_DOUBLE_EQ(w.hz(5.0), 1.0);
}

TEST(RateWindow, OldArrivalsFallOutOfWindow)
{
  RateWindow w(5.0);
  w.start(0.0);
  w.record(1.0);   // will age out by t=7
  w.record(6.0);
  w.record(7.0);
  // At t=7, window is (2, 7]; only the 6.0 and 7.0 arrivals count => 2/5.
  EXPECT_DOUBLE_EQ(w.hz(7.0), 0.4);
}

TEST(RateWindow, SilentWindowReadsZeroHz)
{
  RateWindow w(5.0);
  w.start(0.0);
  w.record(1.0);
  EXPECT_DOUBLE_EQ(w.hz(10.0), 0.0);   // nothing in (5, 10]
  EXPECT_TRUE(w.known(10.0));          // ...and the window has elapsed => Dead
}
