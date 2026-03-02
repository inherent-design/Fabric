#include "fabric/core/InputAxis.hh"
#include <cmath>
#include <gtest/gtest.h>
#include <limits>

using namespace fabric;

// Dead zone

TEST(InputAxisTest, DeadZoneZerosSmallValues) {
    EXPECT_FLOAT_EQ(applyDeadZone(0.1f, 0.2f), 0.0f);
    EXPECT_FLOAT_EQ(applyDeadZone(-0.1f, 0.2f), 0.0f);
    EXPECT_FLOAT_EQ(applyDeadZone(0.0f, 0.2f), 0.0f);
}

TEST(InputAxisTest, DeadZoneRemapsAboveThreshold) {
    // 0.6 with dead zone 0.2: (0.6 - 0.2) / (1.0 - 0.2) = 0.5
    EXPECT_NEAR(applyDeadZone(0.6f, 0.2f), 0.5f, 1e-5f);
    EXPECT_NEAR(applyDeadZone(-0.6f, 0.2f), -0.5f, 1e-5f);

    // Full deflection should map to 1.0
    EXPECT_NEAR(applyDeadZone(1.0f, 0.2f), 1.0f, 1e-5f);
}

TEST(InputAxisTest, DeadZonePassthroughWhenZero) {
    EXPECT_FLOAT_EQ(applyDeadZone(0.5f, 0.0f), 0.5f);
    EXPECT_FLOAT_EQ(applyDeadZone(-0.75f, 0.0f), -0.75f);
}

// Response curves

TEST(InputAxisTest, ResponseCurveLinear) {
    EXPECT_FLOAT_EQ(applyResponseCurve(0.5f, ResponseCurve::Linear), 0.5f);
    EXPECT_FLOAT_EQ(applyResponseCurve(-0.5f, ResponseCurve::Linear), -0.5f);
}

TEST(InputAxisTest, ResponseCurveQuadratic) {
    // sign(0.5) * 0.5^2 = 0.25
    EXPECT_FLOAT_EQ(applyResponseCurve(0.5f, ResponseCurve::Quadratic), 0.25f);
    // sign(-0.5) * 0.5^2 = -0.25 (preserves sign)
    EXPECT_FLOAT_EQ(applyResponseCurve(-0.5f, ResponseCurve::Quadratic), -0.25f);
}

TEST(InputAxisTest, ResponseCurveCubic) {
    // 0.5^3 = 0.125
    EXPECT_FLOAT_EQ(applyResponseCurve(0.5f, ResponseCurve::Cubic), 0.125f);
    // (-0.5)^3 = -0.125 (naturally sign-preserving)
    EXPECT_FLOAT_EQ(applyResponseCurve(-0.5f, ResponseCurve::Cubic), -0.125f);
}

// Full pipeline

TEST(InputAxisTest, ProcessAxisValuePipeline) {
    AxisBinding binding;
    binding.name = "look_x";
    binding.deadZone = 0.2f;
    binding.responseCurve = ResponseCurve::Quadratic;

    // Raw 0.6 -> dead zone remap: 0.5 -> quadratic: 0.25
    EXPECT_NEAR(processAxisValue(0.6f, binding), 0.25f, 1e-5f);
}

TEST(InputAxisTest, ProcessAxisValueInversion) {
    AxisBinding binding;
    binding.name = "look_y";
    binding.inverted = true;

    EXPECT_FLOAT_EQ(processAxisValue(0.5f, binding), -0.5f);
    EXPECT_FLOAT_EQ(processAxisValue(-0.5f, binding), 0.5f);
}

TEST(InputAxisTest, ProcessAxisValueClamp) {
    AxisBinding binding;
    binding.name = "trigger";
    binding.rangeMin = 0.0f;
    binding.rangeMax = 1.0f;

    EXPECT_FLOAT_EQ(processAxisValue(-0.5f, binding), 0.0f);
    EXPECT_FLOAT_EQ(processAxisValue(1.5f, binding), 1.0f);
    EXPECT_FLOAT_EQ(processAxisValue(0.7f, binding), 0.7f);
}

// Data types

TEST(InputAxisTest, KeyPairSourceConstruction) {
    KeyPairSource kp;
    kp.negative = SDLK_A;
    kp.positive = SDLK_D;

    KeyPairSource kp2{SDLK_A, SDLK_D};
    EXPECT_EQ(kp, kp2);
}

TEST(InputAxisTest, AxisBindingDefaults) {
    AxisBinding binding;

    EXPECT_FLOAT_EQ(binding.deadZone, 0.0f);
    EXPECT_EQ(binding.responseCurve, ResponseCurve::Linear);
    EXPECT_FALSE(binding.inverted);
    EXPECT_FLOAT_EQ(binding.rangeMin, -1.0f);
    EXPECT_FLOAT_EQ(binding.rangeMax, 1.0f);
}

TEST(InputAxisTest, AxisSourceDefaults) {
    AxisSource as;

    EXPECT_FALSE(as.useKeyPair);
    EXPECT_FLOAT_EQ(as.scale, 1.0f);
}

// --- Audit: dead zone boundary values ---

TEST(InputAxisTest, DeadZoneBoundaryExactlyAtThreshold) {
    EXPECT_FLOAT_EQ(applyDeadZone(0.2f, 0.2f), 0.0f);
    EXPECT_FLOAT_EQ(applyDeadZone(-0.2f, 0.2f), 0.0f);
}

TEST(InputAxisTest, DeadZoneJustAboveThreshold) {
    float result = applyDeadZone(0.21f, 0.2f);
    EXPECT_GT(result, 0.0f);
    EXPECT_LT(result, 0.05f);

    float negResult = applyDeadZone(-0.21f, 0.2f);
    EXPECT_LT(negResult, 0.0f);
    EXPECT_GT(negResult, -0.05f);
}

TEST(InputAxisTest, DeadZoneAtOrAboveOne) {
    EXPECT_FLOAT_EQ(applyDeadZone(0.5f, 1.0f), 0.0f);
    EXPECT_FLOAT_EQ(applyDeadZone(1.0f, 1.0f), 0.0f);
    EXPECT_FLOAT_EQ(applyDeadZone(-1.0f, 1.0f), 0.0f);
    EXPECT_FLOAT_EQ(applyDeadZone(0.99f, 1.5f), 0.0f);
}

TEST(InputAxisTest, DeadZoneNegativePassesThrough) {
    EXPECT_FLOAT_EQ(applyDeadZone(0.5f, -0.1f), 0.5f);
    EXPECT_FLOAT_EQ(applyDeadZone(-0.5f, -0.1f), -0.5f);
}

// --- Audit: response curve extremes ---

TEST(InputAxisTest, ResponseCurveExtremesLinear) {
    EXPECT_FLOAT_EQ(applyResponseCurve(0.0f, ResponseCurve::Linear), 0.0f);
    EXPECT_FLOAT_EQ(applyResponseCurve(1.0f, ResponseCurve::Linear), 1.0f);
    EXPECT_FLOAT_EQ(applyResponseCurve(-1.0f, ResponseCurve::Linear), -1.0f);
}

TEST(InputAxisTest, ResponseCurveExtremesQuadratic) {
    EXPECT_FLOAT_EQ(applyResponseCurve(0.0f, ResponseCurve::Quadratic), 0.0f);
    EXPECT_FLOAT_EQ(applyResponseCurve(1.0f, ResponseCurve::Quadratic), 1.0f);
    EXPECT_FLOAT_EQ(applyResponseCurve(-1.0f, ResponseCurve::Quadratic), -1.0f);
}

TEST(InputAxisTest, ResponseCurveExtremesCubic) {
    EXPECT_FLOAT_EQ(applyResponseCurve(0.0f, ResponseCurve::Cubic), 0.0f);
    EXPECT_FLOAT_EQ(applyResponseCurve(1.0f, ResponseCurve::Cubic), 1.0f);
    EXPECT_FLOAT_EQ(applyResponseCurve(-1.0f, ResponseCurve::Cubic), -1.0f);
}

TEST(InputAxisTest, ResponseCurveBeyondOneQuadratic) {
    EXPECT_FLOAT_EQ(applyResponseCurve(2.0f, ResponseCurve::Quadratic), 4.0f);
    EXPECT_FLOAT_EQ(applyResponseCurve(-2.0f, ResponseCurve::Quadratic), -4.0f);
}

TEST(InputAxisTest, ResponseCurveBeyondOneCubic) {
    EXPECT_FLOAT_EQ(applyResponseCurve(2.0f, ResponseCurve::Cubic), 8.0f);
    EXPECT_FLOAT_EQ(applyResponseCurve(-2.0f, ResponseCurve::Cubic), -8.0f);
}

// --- Audit: inversion combined with dead zone ---

TEST(InputAxisTest, InversionCombinedWithDeadZone) {
    AxisBinding binding;
    binding.name = "test";
    binding.deadZone = 0.2f;
    binding.inverted = true;

    EXPECT_NEAR(processAxisValue(0.6f, binding), -0.5f, 1e-5f);
    EXPECT_NEAR(processAxisValue(-0.6f, binding), 0.5f, 1e-5f);
    EXPECT_FLOAT_EQ(processAxisValue(0.1f, binding), 0.0f);
}

// --- Audit: extreme input values ---

TEST(InputAxisTest, ProcessAxisInfinity) {
    AxisBinding binding;
    binding.name = "test";

    float posResult = processAxisValue(std::numeric_limits<float>::infinity(), binding);
    EXPECT_FLOAT_EQ(posResult, 1.0f);

    float negResult = processAxisValue(-std::numeric_limits<float>::infinity(), binding);
    EXPECT_FLOAT_EQ(negResult, -1.0f);
}

// --- Audit: full pipeline variants ---

TEST(InputAxisTest, FullPipelineCubicWithInversion) {
    AxisBinding binding;
    binding.name = "test";
    binding.deadZone = 0.1f;
    binding.responseCurve = ResponseCurve::Cubic;
    binding.inverted = true;

    EXPECT_NEAR(processAxisValue(1.0f, binding), -1.0f, 1e-5f);
}

TEST(InputAxisTest, KeyPairSourceEquality) {
    KeyPairSource a{SDLK_A, SDLK_D};
    KeyPairSource b{SDLK_A, SDLK_D};
    KeyPairSource c{SDLK_S, SDLK_W};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(InputAxisTest, KeyPairSourceDefaultEquality) {
    KeyPairSource a;
    KeyPairSource b;
    EXPECT_EQ(a, b);
}

TEST(InputAxisTest, TriggerRangeClamp) {
    AxisBinding binding;
    binding.name = "trigger";
    binding.rangeMin = 0.0f;
    binding.rangeMax = 1.0f;
    binding.deadZone = 0.1f;

    EXPECT_FLOAT_EQ(processAxisValue(0.05f, binding), 0.0f);
    EXPECT_GT(processAxisValue(0.5f, binding), 0.0f);
    EXPECT_LE(processAxisValue(0.5f, binding), 1.0f);
}

// --- BUG-NAN: NaN input must not propagate through axis pipeline ---

TEST(InputAxisTest, ProcessAxisNaNReturnsZero) {
    AxisBinding binding;
    binding.name = "test";
    binding.deadZone = 0.2f;

    float nan = std::numeric_limits<float>::quiet_NaN();
    float result = processAxisValue(nan, binding);
    EXPECT_FALSE(std::isnan(result));
    EXPECT_FLOAT_EQ(result, 0.0f);
}
