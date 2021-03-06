#include <memory>
#include <vector>

#include "image_model/additive_noise_module.h"
#include "image_model/blur_module.h"
#include "image_model/downsampling_module.h"
#include "image_model/image_model.h"
#include "image_model/motion_module.h"
#include "motion/motion_shift.h"
#include "util/matrix_util.h"
#include "util/test_util.h"

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using super_resolution::test::AreMatricesEqual;
using testing::_;
using testing::Return;

const cv::Mat kSmallTestImage = (cv::Mat_<double>(4, 6)
    << 1, 2, 3, 4, 5, 6,
       7, 8, 9, 0, 1, 2,
       9, 7, 5, 4, 2, 1,
       2, 4, 6, 8, 0, 1);
const cv::Size kSmallTestImageSize = cv::Size(6, 4);  // 24 pixels total

// Mock the DegradationOperator
class MockDegradationOperator : public super_resolution::DegradationOperator {
 public:
  // We have to mock this because it's pure virtual.
  MOCK_CONST_METHOD2(
      ApplyToImage,
      void(super_resolution::ImageData* image_data, const int index));

  // We have to mock this because it's pure virtual.
  MOCK_CONST_METHOD2(
      ApplyTransposeToImage,
      void(super_resolution::ImageData* image_data, const int index));

  // Returns a cv::Mat degradation operator in matrix form.
  MOCK_CONST_METHOD2(
      GetOperatorMatrix, cv::Mat(const cv::Size& image_size, const int index));
};

// Tests the static function(s) in DegradationOperator.
TEST(ImageModel, DegradationOperator) {
  const cv::Mat kernel = (cv::Mat_<double>(3, 3)
      << -1, 0, 1,
         -2, 0, 2,
         -1, 0, 1);
  const cv::Mat test_image = (cv::Mat_<double>(2, 3)
      << 1, 3, 5,
         9, 5, 2);
  const cv::Mat operator_matrix =
      super_resolution::DegradationOperator::ConvertKernelToOperatorMatrix(
          kernel, test_image.size());

  // Make sure we get the correct kernel.
  const cv::Mat expected_matrix = (cv::Mat_<double>(6, 6)
      << 0,  2,  0,  0,  1,  0,
         -2, 0,  2,  -1, 0,  1,
         0,  -2, 0,  0,  -1, 0,
         0,  1,  0,  0,  2,  0,
         -1, 0,  1,  -2, 0,  2,
         0,  -1, 0,  0,  -2, 0);
  EXPECT_TRUE(AreMatricesEqual(operator_matrix, expected_matrix));

  // Now make sure that we get the correct image after multiplication.
  const cv::Mat test_image_vector = test_image.reshape(1, 6);
  const cv::Mat expected_result = (cv::Mat_<double>(6, 1)
    << 11, 1,   -11,
       13, -10, -13);
  EXPECT_TRUE(AreMatricesEqual(
      operator_matrix * test_image_vector, expected_result));
}

TEST(ImageModel, AdditiveNoiseModule) {
  // Patch radius for single pixel degradation should be 0 since it only needs
  // the pixel value itself.
  const super_resolution::AdditiveNoiseModule additive_noise_module(5);
  // TODO: implement tests.
}

TEST(ImageModel, DownsamplingModule) {
  /* Verify functionality of downsampling a single patch. */

  const super_resolution::DownsamplingModule downsampling_module(2);

  // Check that we can downsample a 3x3 patch correctly with different image
  // indices.
  // TODO: support for patches larger than 3x3 or downsampling scales other
  // than 2 are not yet implemented and will need testing later.
  const cv::Mat test_patch = (cv::Mat_<double>(3, 3)
      << 0.2, 0.5, 0.7,
         0.1, 0.4, 0.1,
         0.9, 0.8, 0.6);

  // Downsampling at index 6 = position (1, 1) of the 5x5 image should result
  // in pixel 0.2 being selected as it is at an even x, y coordinate.
  //
  // See below, the pixel at index 6 is the "?" which the 3x3 patch is centered
  // on. Thus, the closest downsampling sample pixel is the "x" in the top-left
  // corner of the patch, which has a value of 0.2. The "x" pixels are every 2
  // pixels starting at (0, 0) since the downsampling process chooses every
  // other pixel when scale = 2.
  //   # X |   | x #   | x |
  //   #   | ? |   #   |   |
  //   # x |   | x #   | x |
  //   |   |   |   |   |   |
  //   | x |   | x |   | x |
  const cv::Mat expected_patch_1 = (cv::Mat_<double>(1, 1) << 0.2);
/*  const cv::Mat output_patch_1 =
      downsampling_module.ApplyToPatch(test_patch, 0, 0, 6);
  EXPECT_TRUE(AreMatricesEqual(output_patch_1, expected_patch_1));

  // Try index 0 = position (0, 0); should be 0.4 (center pixel), since that
  // pixel is the first sample pixel. This also verifies the edge case.
  const cv::Mat expected_patch_2 = (cv::Mat_<double>(1, 1) << 0.4);
  const cv::Mat output_patch_2 =
      downsampling_module.ApplyToPatch(test_patch, 0, 0, 0);
  EXPECT_TRUE(AreMatricesEqual(output_patch_2, expected_patch_2));

  // Try index 8 = position (1, 3); should be 0.2.
  //   | x |   # X |   | x #
  //   |   |   #   | ? |   #
  //   | x |   # x |   | x #
  //   |   |   |   |   |   |
  //   | x |   | x |   | x |
  const cv::Mat expected_patch_3 = (cv::Mat_<double>(1, 1) << 0.2);
  const cv::Mat output_patch_3 =
      downsampling_module.ApplyToPatch(test_patch, 0, 0, 8);
  EXPECT_TRUE(AreMatricesEqual(output_patch_3, expected_patch_3));

  // Try index 13 = position (2, 3); should be 0.1. Note that the closest "x"
  // (downsampling sample pixel) to the center pixel "?" is the center-left
  // pixel, which has a value of 0.1 in the patch.
  //   | x |   | x |   | x |
  //   |   |   #   |   |   #
  //   | x |   # X | ? | x #
  //   |   |   #   |   |   #
  //   | x |   | x |   | x |
  const cv::Mat expected_patch_4 = (cv::Mat_<double>(1, 1) << 0.1);
  const cv::Mat output_patch_4 =
      downsampling_module.ApplyToPatch(test_patch, 0, 0, 13);
  EXPECT_TRUE(AreMatricesEqual(output_patch_4, expected_patch_4));

  // Try index 17 = position (3, 2); should be 0.5 since the nearest sample "x"
  // is the top-center pixel, which has a value in the patch of 0.5.
  //   | x |   | x |   | x |
  //   |   |   |   |   |   |
  //   | x #   | X |   # x |
  //   |   #   | ? |   #   |
  //   | x #   | x |   # x |
  const cv::Mat expected_patch_5 = (cv::Mat_<double>(1, 1) << 0.5);
  const cv::Mat output_patch_5 =
      downsampling_module.ApplyToPatch(test_patch, 0, 0, 17);
  EXPECT_TRUE(AreMatricesEqual(output_patch_5, expected_patch_5));

  // Finally, try index 24 = position (4, 4); it's centered on a sample pixel,
  // like (0, 0) so the value should be 0.4. This is also a border condition
  // check.
  const cv::Mat expected_patch_6 = (cv::Mat_<double>(1, 1) << 0.4);
  const cv::Mat output_patch_6 =
      downsampling_module.ApplyToPatch(test_patch, 0, 0, 24);
  EXPECT_TRUE(AreMatricesEqual(output_patch_6, expected_patch_6));
*/

  /* Verify that the returned operator matrix is correct. */

  const cv::Mat downsampling_matrix =
      downsampling_module.GetOperatorMatrix(kSmallTestImageSize, 0);

  // 24 pixels in high-res input, 6 (= 24 / 2*2) pixels in downsampled output.
  const cv::Mat expected_matrix = (cv::Mat_<double>(6, 24) <<
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0);

  EXPECT_TRUE(AreMatricesEqual(downsampling_matrix, expected_matrix));

  // Vectorize the test image and compare to the expected outcome.
  const cv::Mat test_image_vector = kSmallTestImage.reshape(1, 24);
  const cv::Mat expected_downsampled_vector = (cv::Mat_<double>(6, 1)
      << 1, 3, 5,
         9, 5, 2);
  EXPECT_TRUE(AreMatricesEqual(
      downsampling_matrix * test_image_vector, expected_downsampled_vector));

  /* Verify that the transpose of downsampling results in the valid image. */

  const cv::Mat expected_upsampled_image = (cv::Mat_<double>(8, 12)
      << 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         7, 0, 8, 0, 9, 0, 0, 0, 1, 0, 2, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         9, 0, 7, 0, 5, 0, 4, 0, 2, 0, 1, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         2, 0, 4, 0, 6, 0, 8, 0, 0, 0, 1, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  // Compute the upsampling using the matrix transpose to verify that the
  // expected matrix is in fact accurate.
  const cv::Mat upsampling_matrix =
      downsampling_module.GetOperatorMatrix(cv::Size(12, 8), 0).t();
  EXPECT_EQ(upsampling_matrix.size(), cv::Size(24, 96));  // cols, rows

  cv::Mat matrix_upsampled = upsampling_matrix * test_image_vector;
  matrix_upsampled = matrix_upsampled.reshape(1, 8);

  EXPECT_TRUE(AreMatricesEqual(
      matrix_upsampled, expected_upsampled_image));

  // Now test the algorithmic transpose function.
  super_resolution::ImageData upsampled_image(
      kSmallTestImage, super_resolution::DO_NOT_NORMALIZE_IMAGE);
  downsampling_module.ApplyTransposeToImage(&upsampled_image, 0);

  EXPECT_TRUE(AreMatricesEqual(
      upsampled_image.GetChannelImage(0), expected_upsampled_image));
}

// Tests the implemented functionality of the MotionModule.
TEST(ImageModel, MotionModule) {
  /* Verify that the returned patch radius is correct. */

  super_resolution::MotionShiftSequence motion_shift_sequence({
    super_resolution::MotionShift(0, 0),
    super_resolution::MotionShift(1, 1),
    super_resolution::MotionShift(-1, 0)
  });
  const super_resolution::MotionModule motion_module(motion_shift_sequence);

  /* Verify that ApplyToPatch correctly applies motion as expected. */

  const cv::Mat input_patch_3x3 = (cv::Mat_<double>(3, 3)
      <<  0.4, 0.75, 0.35,
         0.85,  0.9, 0.01,
          0.3, 0.15, 0.55);

  // For motion shift (0, 0), nothing should move. Since radius is 1, expect a
  // single-value matrix containing the middle element, 0.9.
  const cv::Mat expected_patch_1 = (cv::Mat_<double>(1, 1) << 0.9);
/*  const cv::Mat returned_patch_1 =
      motion_module.ApplyToPatch(input_patch_3x3, 0, 0, 0);
  EXPECT_TRUE(AreMatricesEqual(expected_patch_1, returned_patch_1));

  // For motion shift (1, 1), expect the middle pixel to be shifted down from
  // the top-left corner.
  const cv::Mat expected_patch_2 = (cv::Mat_<double>(1, 1) << 0.4);
  const cv::Mat returned_patch_2 =
      motion_module.ApplyToPatch(input_patch_3x3, 1, 0, 0);
  EXPECT_TRUE(AreMatricesEqual(expected_patch_2, returned_patch_2));

  const cv::Mat input_patch_5x5 = (cv::Mat_<double>(5, 5)
      << 0.33,  0.4, 0.75, 0.35,  0.2,
         0.61, 0.62, 0.63, 0.64, 0.65,
         0.99, 0.85,  0.9, 0.01, 0.78,
          0.1,  0.3, 0.15, 0.55,  0.5,
         0.14, 0.24, 0.34, 0.44, 0.54);

  // Similarly, for (0, 0) expect nothing to move, but this time the returned
  // patch should be 3x3.
  const cv::Mat expected_patch_3 = (cv::Mat_<double>(3, 3)
      << 0.62, 0.63, 0.64,
         0.85,  0.9, 0.01,
          0.3, 0.15, 0.55);
  const cv::Mat returned_patch_3 =
      motion_module.ApplyToPatch(input_patch_5x5, 0, 0, 0);
  EXPECT_TRUE(AreMatricesEqual(expected_patch_3, returned_patch_3));

  // And lastly, for the third shift, verify that we get a shift to the left.
  const cv::Mat expected_patch_4 = (cv::Mat_<double>(3, 3)
      << 0.63, 0.64, 0.65,
          0.9, 0.01, 0.78,
         0.15, 0.55,  0.5);
  const cv::Mat returned_patch_4 =
      motion_module.ApplyToPatch(input_patch_5x5, 2, 0, 0);
  EXPECT_TRUE(AreMatricesEqual(expected_patch_4, returned_patch_4));
*/

  /* Verify that the correct motion operator matrices are returned. */

  // Trivial case: MotionShift(0, 0) should be the identity.
  const cv::Size image_size(3, 3);
  const cv::Mat motion_matrix_1 =
      motion_module.GetOperatorMatrix(image_size, 0);
  const cv::Mat expected_matrix_1 =
      cv::Mat::eye(9, 9, super_resolution::util::kOpenCvMatrixType);
  EXPECT_TRUE(AreMatricesEqual(motion_matrix_1, expected_matrix_1));

  // MotionShift(1, 1) should shift every pixel down and to the right, leaving
  // pixel indices 0, 1, 2, 3, and 6 empty:
  //
  //   | a | b | c |      |   |   |   |
  //   | d | e | f |  =>  |   | a | b |
  //   | g | h | i |      |   | d | e |
  //
  // Hence, given row-first indexing:
  //   'a' moves from index 0 to 4,
  //   'b' moves from index 1 to 5,
  //   'd' moves from index 3 to 7, and
  //   'a' moves from index 4 to 8.
  //
  // The operation matrix represents the pixel value of the output at each
  // pixel index by row; this rows 0, 1, 2, 3, and 6 are all 0 as they map to
  // no pixels in the original image. Row 4 has a 1 in column 0 so it gets
  // the pixel value at index 0 of the original image, and so on.
  const cv::Mat expected_matrix_2 = (cv::Mat_<double>(9, 9)
      << 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0,
         1, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 1, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 1, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 1, 0, 0, 0, 0);
  const cv::Mat motion_matrix_2 =
      motion_module.GetOperatorMatrix(image_size, 1);
  EXPECT_TRUE(AreMatricesEqual(motion_matrix_2, expected_matrix_2));

  // MotionShift(-1, 0) shifts the X axis (columns) by -1 as follows:
  //
  //   | a | b | c |      | b | c |   |
  //   | d | e | f |  =>  | e | f |   |
  //   | g | h | i |      | h | i |   |
  //
  // Thus, the expected matrix is as follows:
  const cv::Mat expected_matrix_3 = (cv::Mat_<double>(9, 9)
      << 0, 1, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 1, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 1, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 1, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 1, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 1,
         0, 0, 0, 0, 0, 0, 0, 0, 0);
  const cv::Mat motion_matrix_3 =
      motion_module.GetOperatorMatrix(image_size, 2);
  EXPECT_TRUE(AreMatricesEqual(motion_matrix_3, expected_matrix_3));
}

TEST(ImageModel, BlurModule) {
  /* Verify that blur operator works as expected. */

  // For a 3x3 kernel, a sigma of 0.849321 is almost exactly the "standard"
  // kernel:
  //   | 0.0625 | 0.125  | 0.0625 |     | 1/16 | 1/8  | 1/16 |
  //   | 0.125  |  0.25  | 0.125  |  =  | 1/8  | 1/4  | 1/8  |
  //   | 0.0625 | 0.125  | 0.0625 |     | 1/16 | 1/8  | 1/16 |
  const super_resolution::BlurModule blur_module(3, 0.849321);

  // The expected kSmallTestImage after being blurred with the standard kernel.
  // This result was generated with a separate script.
  const cv::Mat expected_blurred_image = (cv::Mat_<double>(4, 6)
      << 1.875,   3.0,    3.125,   2.625,   2.75,    2.4375,
         4.5625,  6.25,   5.3125,  3.1875,  2.3125,  1.9375,
         5.0,     6.5,    5.75,    3.875,   1.9375,  0.9375,
         2.5625,  3.75,   4.3125,  3.6875,  1.6875,  0.5);

  super_resolution::ImageData image_data(
      kSmallTestImage, super_resolution::DO_NOT_NORMALIZE_IMAGE);
  blur_module.ApplyToImage(&image_data, 0);

  const double diff_tolerance = 0.001;
  EXPECT_TRUE(AreMatricesEqual(
      image_data.GetChannelImage(0), expected_blurred_image, diff_tolerance));

  // Also verify that we get the right results when using the matrix version.
  const cv::Mat blur_matrix =
      blur_module.GetOperatorMatrix(kSmallTestImage.size(), 0);

  const cv::Mat test_image_vector = kSmallTestImage.reshape(1, 24);
  const cv::Mat blurred_test_image_vector = blur_matrix * test_image_vector;
  const cv::Mat blurred_test_image =
      blurred_test_image_vector.reshape(1, 4);

  EXPECT_TRUE(AreMatricesEqual(
      blurred_test_image, expected_blurred_image, diff_tolerance));

  /* Now verify that the transpose operator works as expected. */

  // Since the Gaussian kernel is symmetric, the resulting image should be
  // the same as the original blurring operation.

  // First check the actual matrix transpose to make sure.
  const cv::Mat transpose_blurred_test_image_vector =
      blur_matrix.t() * test_image_vector;
  const cv::Mat transpose_blurred_test_image =
      transpose_blurred_test_image_vector.reshape(1, 4);
  EXPECT_TRUE(AreMatricesEqual(
      transpose_blurred_test_image, expected_blurred_image, diff_tolerance));

  // Now check that we get the same results if applied to the image using the
  // convolution operator directly.
  super_resolution::ImageData image_data2(
      kSmallTestImage, super_resolution::DO_NOT_NORMALIZE_IMAGE);
  blur_module.ApplyTransposeToImage(&image_data2, 0);
  EXPECT_TRUE(AreMatricesEqual(
      image_data2.GetChannelImage(0), expected_blurred_image, diff_tolerance));
}

// Tests that both the ApplyToImage and the ApplyToPixel methods correctly
// return the right values of the degraded image. This does not test the
// method's efficiency, but verifies its correctness and compares the two
// functions to make sure they both return the same values.
// TODO: implement ApplyTransposeToImage test.
TEST(ImageModel, ApplyToImage) {
  // TODO: finish implementing this and fix the test.
  /*
  const super_resolution::ImageData input_image(kSmallTestImage);

  std::unique_ptr<MockDegradationOperator> mock_operator(
      new MockDegradationOperator());
  EXPECT_CALL(*mock_operator, ApplyToImage(_, 0))
      .Times(4);  // TODO: this is the current implementation.

  super_resolution::ImageModel image_model(2);
  image_model.AddDegradationOperator(std::move(mock_operator));

  const double pixel_0 = image_model.ApplyToPixel(input_image, 0, 0, 0);
  EXPECT_EQ(pixel_0, input_image.GetPixelValue(0, 0));

  const double pixel_1 = image_model.ApplyToPixel(input_image, 0, 0, 1);
  EXPECT_EQ(pixel_1, input_image.GetPixelValue(0, 1));

  const double pixel_4 = image_model.ApplyToPixel(input_image, 0, 0, 4);
  EXPECT_EQ(pixel_4, input_image.GetPixelValue(0, 4));

  const double pixel_9 = image_model.ApplyToPixel(input_image, 0, 0, 9);
  EXPECT_EQ(pixel_9, input_image.GetPixelValue(0, 9));
  */
}

// Tests that the GetModelMatrix method correctly returns the appropriately
// multiplied degradation matrices.
TEST(ImageModel, GetModelMatrix) {
  super_resolution::ImageModel image_model(2);
  const cv::Size image_size(2, 2);

  const cv::Mat operator_matrix_1 = (cv::Mat_<double>(4, 4)
      << 0, 0, 0, -3,
         4, 3, 2, 1,
         3, 1, 4, 9,
         1, 0, 0, 1);
  std::shared_ptr<MockDegradationOperator> mock_operator_1(
      new MockDegradationOperator());
  EXPECT_CALL(*mock_operator_1, GetOperatorMatrix(image_size, 0))
      .WillOnce(Return(operator_matrix_1));

  const cv::Mat operator_matrix_2 = (cv::Mat_<double>(4, 4)
      << 0, 2, 0, 5,
         1, 1, 1, 1,
         0, 0, 0, 0,
         1, 2, 3, -4);
  std::shared_ptr<MockDegradationOperator> mock_operator_2(
      new MockDegradationOperator());
  EXPECT_CALL(*mock_operator_2, GetOperatorMatrix(image_size, 0))
      .WillOnce(Return(operator_matrix_2));

  const cv::Mat operator_matrix_3 = (cv::Mat_<double>(3, 4)
      << 1, 0, 0, 0,
         0, 1, 0, 0,
         0, 0, 1, 0);
  std::shared_ptr<MockDegradationOperator> mock_operator_3(
      new MockDegradationOperator());
  EXPECT_CALL(*mock_operator_3, GetOperatorMatrix(image_size, 0))
      .WillOnce(Return(operator_matrix_3));

  image_model.AddDegradationOperator(mock_operator_1);
  image_model.AddDegradationOperator(mock_operator_2);
  image_model.AddDegradationOperator(mock_operator_3);

  // op3 * (op2 * op1)
  const cv::Mat expected_result = (cv::Mat_<double>(3, 4)
      << 13, 6, 4, 7,
          8, 4, 6, 8,
          0, 0, 0, 0);
  cv::Mat returned_operator_matrix = image_model.GetModelMatrix(image_size, 0);
  EXPECT_TRUE(AreMatricesEqual(returned_operator_matrix, expected_result));
}
