#include "core/assert.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

using frankie::assert_detail::assert_level;
using frankie::assert_detail::binary_expression;
using frankie::assert_detail::decomposer;
using frankie::assert_detail::expression_value;
using frankie::assert_detail::has_format_details_v;
using frankie::assert_detail::op_eq;
using frankie::assert_detail::op_ge;
using frankie::assert_detail::op_gt;
using frankie::assert_detail::op_le;
using frankie::assert_detail::op_lt;
using frankie::assert_detail::op_ne;
using frankie::assert_detail::set_failure_handler;
using frankie::assert_detail::source_location;

namespace {

constexpr std::uint64_t k_buf_size{64};

std::array<char, k_buf_size> make_buf() {
  std::array<char, k_buf_size> buf{};
  std::memset(buf.data(), '\0', k_buf_size);
  return buf;
}

}  // namespace

// ============================================================================
// print_value
// ============================================================================

TEST(AssertPrintValueTest, Bool) {
  auto buf = make_buf();
  frankie::assert_detail::print_value(buf.data(), k_buf_size, true);
  EXPECT_STREQ(buf.data(), "true");

  buf = make_buf();
  frankie::assert_detail::print_value(buf.data(), k_buf_size, false);
  EXPECT_STREQ(buf.data(), "false");
}

TEST(AssertPrintValueTest, Char) {
  auto buf = make_buf();
  frankie::assert_detail::print_value(buf.data(), k_buf_size, 'l');
  EXPECT_STREQ(buf.data(), "'l' (0x6c)");

  buf = make_buf();
  frankie::assert_detail::print_value(buf.data(), k_buf_size, 'A');
  EXPECT_STREQ(buf.data(), "'A' (0x41)");
}

TEST(AssertPrintValueTest, SignedInteger) {
  auto buf = make_buf();
  frankie::assert_detail::print_value(buf.data(), k_buf_size, -4);
  EXPECT_STREQ(buf.data(), "-4");

  buf = make_buf();
  frankie::assert_detail::print_value(buf.data(), k_buf_size, static_cast<std::int64_t>(-1234567890123LL));
  EXPECT_STREQ(buf.data(), "-1234567890123");
}

TEST(AssertPrintValueTest, UnsignedInteger) {
  auto buf = make_buf();
  frankie::assert_detail::print_value(buf.data(), k_buf_size, 5u);
  EXPECT_STREQ(buf.data(), "5");

  buf = make_buf();
  frankie::assert_detail::print_value(buf.data(), k_buf_size, static_cast<std::uint64_t>(18446744073709551614ULL));
  EXPECT_STREQ(buf.data(), "18446744073709551614");
}

TEST(AssertPrintValueTest, FloatingPoint) {
  auto buf = make_buf();
  frankie::assert_detail::print_value(buf.data(), k_buf_size, 0.549);
  EXPECT_STREQ(buf.data(), "0.549");

  buf = make_buf();
  frankie::assert_detail::print_value(buf.data(), k_buf_size, 1.5f);
  EXPECT_STREQ(buf.data(), "1.5");
}

TEST(AssertPrintValueTest, ConstCharPointer) {
  auto buf = make_buf();
  const char* msg = "hello";
  frankie::assert_detail::print_value(buf.data(), k_buf_size, msg);
  EXPECT_STREQ(buf.data(), "\"hello\"");
}

TEST(AssertPrintValueTest, MutableCharPointer) {
  auto buf = make_buf();
  char msg[] = "world";
  char* ptr = msg;
  frankie::assert_detail::print_value(buf.data(), k_buf_size, ptr);
  EXPECT_STREQ(buf.data(), "\"world\"");
}

TEST(AssertPrintValueTest, NullConstCharPointer) {
  auto buf = make_buf();
  const char* msg = nullptr;
  frankie::assert_detail::print_value(buf.data(), k_buf_size, msg);
  EXPECT_STREQ(buf.data(), "nullptr");
}

TEST(AssertPrintValueTest, NullVoidPointer) {
  auto buf = make_buf();
  void* ptr = nullptr;
  frankie::assert_detail::print_value(buf.data(), k_buf_size, ptr);
  EXPECT_STREQ(buf.data(), "nullptr");
}

TEST(AssertPrintValueTest, NonNullVoidPointer) {
  auto buf = make_buf();
  int x = 42;
  void* ptr = &x;
  frankie::assert_detail::print_value(buf.data(), k_buf_size, ptr);
  EXPECT_GT(std::strlen(buf.data()), 0u);
  EXPECT_STRNE(buf.data(), "nullptr");
}

TEST(AssertPrintValueTest, Unprintable) {
  struct unprintable {};
  auto buf = make_buf();
  frankie::assert_detail::print_value(buf.data(), k_buf_size, unprintable{});
  EXPECT_STREQ(buf.data(), "<unprintable>");
}

// ============================================================================
// expression_value
// ============================================================================

TEST(AssertExpressionValueTest, BoolConversionFromBool) {
  expression_value<bool> e_true{true};
  EXPECT_TRUE(static_cast<bool>(e_true));

  expression_value<bool> e_false{false};
  EXPECT_FALSE(static_cast<bool>(e_false));
}

TEST(AssertExpressionValueTest, BoolConversionFromInt) {
  expression_value<int> e_zero{0};
  EXPECT_FALSE(static_cast<bool>(e_zero));

  expression_value<int> e_one{1};
  EXPECT_TRUE(static_cast<bool>(e_one));
}

TEST(AssertExpressionValueTest, BoolConversionFromNonConvertibleIsTrue) {
  struct not_bool {};
  expression_value<not_bool> e{not_bool{}};
  EXPECT_TRUE(static_cast<bool>(e));
}

TEST(AssertExpressionValueTest, EqualsOperator) {
  EXPECT_TRUE(static_cast<bool>(expression_value<int>{5} == 5));
  EXPECT_FALSE(static_cast<bool>(expression_value<int>{5} == 10));
}

TEST(AssertExpressionValueTest, NotEqualsOperator) {
  EXPECT_TRUE(static_cast<bool>(expression_value<int>{5} != 10));
  EXPECT_FALSE(static_cast<bool>(expression_value<int>{5} != 5));
}

TEST(AssertExpressionValueTest, LessThanOperator) {
  EXPECT_TRUE(static_cast<bool>(expression_value<int>{3} < 5));
  EXPECT_FALSE(static_cast<bool>(expression_value<int>{5} < 5));
  EXPECT_FALSE(static_cast<bool>(expression_value<int>{5} < 3));
}

TEST(AssertExpressionValueTest, LessOrEqualOperator) {
  EXPECT_TRUE(static_cast<bool>(expression_value<int>{3} <= 5));
  EXPECT_TRUE(static_cast<bool>(expression_value<int>{5} <= 5));
  EXPECT_FALSE(static_cast<bool>(expression_value<int>{6} <= 5));
}

TEST(AssertExpressionValueTest, GreaterThanOperator) {
  EXPECT_TRUE(static_cast<bool>(expression_value<int>{5} > 3));
  EXPECT_FALSE(static_cast<bool>(expression_value<int>{5} > 5));
  EXPECT_FALSE(static_cast<bool>(expression_value<int>{3} > 5));
}

TEST(AssertExpressionValueTest, GreaterOrEqualOperator) {
  EXPECT_TRUE(static_cast<bool>(expression_value<int>{5} >= 3));
  EXPECT_TRUE(static_cast<bool>(expression_value<int>{5} >= 5));
  EXPECT_FALSE(static_cast<bool>(expression_value<int>{3} >= 5));
}

TEST(AssertExpressionValueTest, PrintTo) {
  auto buf = make_buf();
  expression_value<int> e{42};
  e.print_to(buf.data(), k_buf_size);
  EXPECT_STREQ(buf.data(), "42");
}

// ============================================================================
// binary_expression
// ============================================================================

TEST(AssertBinaryExpressionTest, OpStrings) {
  auto eq = expression_value<int>{1} == 1;
  auto ne = expression_value<int>{1} != 2;
  auto lt = expression_value<int>{1} < 2;
  auto le = expression_value<int>{1} <= 2;
  auto gt = expression_value<int>{2} > 1;
  auto ge = expression_value<int>{2} >= 1;

  EXPECT_STREQ(decltype(eq)::op_str(), "==");
  EXPECT_STREQ(decltype(ne)::op_str(), "!=");
  EXPECT_STREQ(decltype(lt)::op_str(), "<");
  EXPECT_STREQ(decltype(le)::op_str(), "<=");
  EXPECT_STREQ(decltype(gt)::op_str(), ">");
  EXPECT_STREQ(decltype(ge)::op_str(), ">=");
}

TEST(AssertBinaryExpressionTest, UnknownOpStrFallback) {
  struct fake_op {};
  using fake_binexpr = binary_expression<expression_value<int>, expression_value<int>, fake_op>;
  EXPECT_STREQ(fake_binexpr::op_str(), "<unrecognized-operation>");
}

TEST(AssertBinaryExpressionTest, FormatDetailsIncludesLhsRhs) {
  constexpr std::uint64_t k_details_size{256};
  std::array<char, k_details_size> details{};
  auto expr = expression_value<int>{5} == 10;
  expr.format_details(details.data(), k_details_size);

  const std::string out{details.data()};
  EXPECT_NE(out.find("LHS: 5"), std::string::npos);
  EXPECT_NE(out.find("RHS: 10"), std::string::npos);
}

TEST(AssertBinaryExpressionTest, ResultReflectsComparison) {
  auto expr_true = expression_value<int>{5} == 5;
  EXPECT_TRUE(static_cast<bool>(expr_true));
  EXPECT_TRUE(expr_true.result);

  auto expr_false = expression_value<int>{5} == 10;
  EXPECT_FALSE(static_cast<bool>(expr_false));
  EXPECT_FALSE(expr_false.result);
}

// ============================================================================
// decomposer
// ============================================================================

TEST(AssertDecomposerTest, CapturesBinaryExpressionResult) {
  auto expr_true = (decomposer{} << 5) == 5;
  EXPECT_TRUE(static_cast<bool>(expr_true));

  auto expr_false = (decomposer{} << 5) == 10;
  EXPECT_FALSE(static_cast<bool>(expr_false));
}

TEST(AssertDecomposerTest, CapturesSingleValue) {
  auto expr_true = decomposer{} << true;
  EXPECT_TRUE(static_cast<bool>(expr_true));

  auto expr_false = decomposer{} << false;
  EXPECT_FALSE(static_cast<bool>(expr_false));
}

// ============================================================================
// has_format_details SFINAE
// ============================================================================

TEST(AssertHasFormatDetailsTest, FalseForExpressionValue) {
  EXPECT_FALSE(has_format_details_v<expression_value<int>>);
}

TEST(AssertHasFormatDetailsTest, FalseForPlainType) {
  EXPECT_FALSE(has_format_details_v<int>);
  EXPECT_FALSE(has_format_details_v<bool>);
}

// Note: this test will fail while the SFINAE in assert.hpp:264 uses
// std::declval<std::uint64_t()>() (a function type) instead of
// std::declval<std::uint64_t>(). That bug suppresses LHS/RHS formatting
// in every failed binary-expression assertion.
TEST(AssertHasFormatDetailsTest, TrueForBinaryExpression) {
  using expr_type = decltype(expression_value<int>{1} == 1);
  EXPECT_TRUE(has_format_details_v<expr_type>);
}

// ============================================================================
// source_location
// ============================================================================

TEST(AssertSourceLocationTest, DefaultCapturesCallSite) {
  source_location loc{};
  EXPECT_NE(loc.file_, nullptr);
  EXPECT_NE(loc.function_, nullptr);
  EXPECT_GT(loc.line_, 0u);
}

#if FR_ASSERT_HAS_SOURCE_LOCATION
TEST(AssertSourceLocationTest, ConstructsFromStdSourceLocation) {
  auto std_loc = std::source_location::current();
  source_location loc{std_loc};
  EXPECT_STREQ(loc.file_, std_loc.file_name());
  EXPECT_EQ(loc.line_, std_loc.line());
}
#endif

// ============================================================================
// check_assertion (passing paths only - failing paths are in death tests)
// ============================================================================

TEST(AssertCheckAssertionTest, PassingPredicateDoesNotAbort) {
  auto expr = decomposer{} << true;
  frankie::assert_detail::check_assertion(assert_level::verify, expr, "true");
  SUCCEED();
}

TEST(AssertCheckAssertionTest, PassingBinaryExpressionDoesNotAbort) {
  auto expr = (decomposer{} << 5) == 5;
  frankie::assert_detail::check_assertion(assert_level::verify, expr, "5 == 5");
  SUCCEED();
}

TEST(AssertCheckAssertionTest, PassingMsgDoesNotAbort) {
  auto expr = (decomposer{} << 1) < 2;
  frankie::assert_detail::check_assertion_msg(assert_level::verify, expr, "1 < 2", "fine");
  SUCCEED();
}

// ============================================================================
// Macros - passing cases
// ============================================================================

TEST(AssertMacroTest, VerifyPassesWithoutAbort) {
  FR_VERIFY(1 == 1);
  FR_VERIFY(true);
  FR_VERIFY_MSG(1 + 1 == 2, "basic math");
  SUCCEED();
}

TEST(AssertMacroTest, DebugAssertPassesWithoutAbort) {
  FR_DEBUG_ASSERT(true);
  FR_DEBUG_ASSERT(1 == 1);
  FR_DEBUG_ASSERT_MSG(2 > 1, "obvious");
  SUCCEED();
}

TEST(AssertMacroTest, AssumePassesWithoutAbort) {
  FR_ASSUME(true);
  FR_ASSUME(1 + 1 == 2);
  FR_ASSUME_MSG(2 > 1, "trivial");
  SUCCEED();
}

TEST(AssertMacroTest, AssertNotNullReturnsPointer) {
  int x = 42;
  int* ptr = &x;
  int* result = FR_ASSERT_NOT_NULL(ptr);
  EXPECT_EQ(result, &x);
  EXPECT_EQ(*result, 42);
}

TEST(AssertMacroTest, AssertNotNullUsableInExpressionContext) {
  int x = 7;
  int* ptr = &x;
  if (int* result = FR_ASSERT_NOT_NULL(ptr)) {
    EXPECT_EQ(*result, 7);
  } else {
    FAIL() << "non-null pointer should have been returned";
  }
}

TEST(AssertMacroTest, AssertValMatchDoesNotAbort) {
  int x = 5;
  FR_ASSERT_VAL(x, 5);
  SUCCEED();
}

// ============================================================================
// Death tests - failing macro paths emit expected diagnostics
// ============================================================================

TEST(AssertDeathTest, VerifyFailureEmitsVerification) {
  EXPECT_DEATH(
      {
        set_failure_handler(nullptr);
        FR_VERIFY(1 == 2);
      },
      "VERIFICATION FAILED");
}

TEST(AssertDeathTest, VerifyMsgIncludesCustomMessage) {
  EXPECT_DEATH(
      {
        set_failure_handler(nullptr);
        FR_VERIFY_MSG(1 == 2, "my custom failure message");
      },
      "my custom failure message");
}

TEST(AssertDeathTest, VerifyFailureIncludesExpressionText) {
  EXPECT_DEATH(
      {
        set_failure_handler(nullptr);
        FR_VERIFY(1 == 2);
      },
      "Expression: 1 == 2");
}

TEST(AssertDeathTest, VerifyMsgSurfacesUnderMessageSection) {
  EXPECT_DEATH(
      {
        set_failure_handler(nullptr);
        FR_VERIFY_MSG(1 == 2, "routed as message");
      },
      "Message: routed as message");
}

TEST(AssertDeathTest, PanicEmitsMessage) {
  EXPECT_DEATH(
      {
        set_failure_handler(nullptr);
        FR_PANIC("something went wrong");
      },
      "PANIC FAILED");
}

TEST(AssertDeathTest, UnreachableAborts) {
  EXPECT_DEATH(
      {
        set_failure_handler(nullptr);
        FR_UNREACHABLE();
      },
      "UNREACHABLE");
}

TEST(AssertDeathTest, NotImplementedAborts) {
  EXPECT_DEATH(
      {
        set_failure_handler(nullptr);
        FR_NOT_IMPLEMENTED();
      },
      "NOT_IMPLEMENTED");
}

TEST(AssertDeathTest, AssertNotNullOnNullAborts) {
  EXPECT_DEATH(
      {
        set_failure_handler(nullptr);
        int* ptr = nullptr;
        FR_ASSERT_NOT_NULL(ptr);
      },
      "Unexpected null pointer");
}

TEST(AssertDeathTest, AssertValMismatchShowsExpectedAndGot) {
  EXPECT_DEATH(
      {
        set_failure_handler(nullptr);
        int x = 5;
        FR_ASSERT_VAL(x, 10);
      },
      "Expected: 10");
}

#if FR_ASSERT_LEVEL >= 3
TEST(AssertDeathTest, DebugAssertFailureEmitsDebugAssert) {
  EXPECT_DEATH(
      {
        set_failure_handler(nullptr);
        FR_DEBUG_ASSERT(false);
      },
      "DEBUG_ASSERT FAILED");
}
#endif

#if FR_ASSERT_LEVEL >= 2
TEST(AssertDeathTest, AssumeFailureEmitsAssumption) {
  EXPECT_DEATH(
      {
        set_failure_handler(nullptr);
        FR_ASSUME(false);
      },
      "ASSUMPTION FAILED");
}
#endif

// ============================================================================
// Death tests - custom failure handler
// ============================================================================

namespace {

void stderr_tagging_handler(assert_level level, const char* expression, const char* message,
                            const source_location& loc) noexcept {
  (void)loc;
  std::fprintf(stderr, "CUSTOM_HANDLER level=%d expr=%s msg=%s\n", static_cast<int>(level),
               expression ? expression : "<null>", message ? message : "<null>");
  std::fflush(stderr);
}

}  // namespace

TEST(AssertDeathTest, CustomFailureHandlerIsInvoked) {
  EXPECT_DEATH(
      {
        set_failure_handler(stderr_tagging_handler);
        FR_VERIFY(1 == 2);
      },
      "CUSTOM_HANDLER");
}

TEST(AssertDeathTest, SetFailureHandlerNullRestoresDefault) {
  EXPECT_DEATH(
      {
        set_failure_handler(stderr_tagging_handler);
        set_failure_handler(nullptr);
        FR_VERIFY(1 == 2);
      },
      "VERIFICATION FAILED");
}
