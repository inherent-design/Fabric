#include "fabric/utils/ErrorHandling.hh"
#include "gtest/gtest.h"
#include <stdexcept>
#include <string>
#include <vector>

// Test fixture for ErrorHandling tests
class ErrorHandlingTest : public ::testing::Test {};

// Test FabricException construction
TEST_F(ErrorHandlingTest, TestFabricExceptionConstruction) {
  fabric::FabricException exception("Test error message");
  ASSERT_STREQ("Test error message", exception.what());
}

// Test throwError function
TEST_F(ErrorHandlingTest, TestThrowError) {
  try {
    fabric::throwError("Test error message");
    FAIL() << "Expected FabricException";
  } catch (const fabric::FabricException &e) {
    ASSERT_STREQ("Test error message", e.what());
  } catch (...) {
    FAIL() << "Expected FabricException, got a different exception";
  }
}

// ErrorCode tests

TEST_F(ErrorHandlingTest, ErrorCodeToString) {
  EXPECT_EQ(fabric::errorCodeToString(fabric::ErrorCode::Ok), "Ok");
  EXPECT_EQ(fabric::errorCodeToString(fabric::ErrorCode::BufferOverrun), "BufferOverrun");
  EXPECT_EQ(fabric::errorCodeToString(fabric::ErrorCode::Timeout), "Timeout");
  EXPECT_EQ(fabric::errorCodeToString(fabric::ErrorCode::NotFound), "NotFound");
  EXPECT_EQ(fabric::errorCodeToString(fabric::ErrorCode::Internal), "Internal");
}

// Result<T> tests

TEST_F(ErrorHandlingTest, ResultOkValue) {
  auto r = fabric::Result<int>::ok(42);
  EXPECT_TRUE(r.isOk());
  EXPECT_FALSE(r.isError());
  EXPECT_EQ(r.code(), fabric::ErrorCode::Ok);
  EXPECT_EQ(r.value(), 42);
}

TEST_F(ErrorHandlingTest, ResultErrorValue) {
  auto r = fabric::Result<int>::error(fabric::ErrorCode::NotFound, "missing");
  EXPECT_FALSE(r.isOk());
  EXPECT_TRUE(r.isError());
  EXPECT_EQ(r.code(), fabric::ErrorCode::NotFound);
  EXPECT_EQ(r.message(), "missing");
}

TEST_F(ErrorHandlingTest, ResultValueThrowsOnError) {
  auto r = fabric::Result<int>::error(fabric::ErrorCode::Internal, "broken");
  EXPECT_THROW(r.value(), fabric::FabricException);
}

TEST_F(ErrorHandlingTest, ResultValueOr) {
  auto ok = fabric::Result<int>::ok(10);
  EXPECT_EQ(ok.valueOr(99), 10);

  auto err = fabric::Result<int>::error(fabric::ErrorCode::Timeout);
  EXPECT_EQ(err.valueOr(99), 99);
}

TEST_F(ErrorHandlingTest, ResultString) {
  auto r = fabric::Result<std::string>::ok("hello");
  EXPECT_EQ(r.value(), "hello");
}

TEST_F(ErrorHandlingTest, ResultMoveOnly) {
  auto r = fabric::Result<std::vector<int>>::ok({1, 2, 3});
  auto moved = std::move(r);
  EXPECT_TRUE(moved.isOk());
  EXPECT_EQ(moved.value().size(), 3u);
}

// Result<void> tests

TEST_F(ErrorHandlingTest, ResultVoidOk) {
  auto r = fabric::Result<void>::ok();
  EXPECT_TRUE(r.isOk());
  EXPECT_FALSE(r.isError());
  EXPECT_EQ(r.code(), fabric::ErrorCode::Ok);
}

TEST_F(ErrorHandlingTest, ResultVoidError) {
  auto r = fabric::Result<void>::error(fabric::ErrorCode::PermissionDenied, "nope");
  EXPECT_TRUE(r.isError());
  EXPECT_EQ(r.code(), fabric::ErrorCode::PermissionDenied);
  EXPECT_EQ(r.message(), "nope");
}
