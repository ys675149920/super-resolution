#include <iostream>  // TODO: remove
#include <vector>

#include "image/image_data.h"
#include "image_model/downsampling_module.h"
#include "image_model/image_model.h"
#include "image_model/motion_module.h"
#include "motion/motion_shift.h"
#include "solvers/map_solver.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

TEST(MapSolver, SmallDataTest) {
  // Create the low-res test images.
  const cv::Mat lr_image_1 = (cv::Mat_<double>(2, 2)
    << 0.5, 0.5,
       0.5, 0.5);
  const cv::Mat lr_image_2 = (cv::Mat_<double>(2, 2)
    << 0.25, 0.25,
       0.25, 0.25);
  std::vector<super_resolution::ImageData> low_res_images {
    super_resolution::ImageData(lr_image_1),
    super_resolution::ImageData(lr_image_2)
  };

  // Create an empty image model.
  super_resolution::ImageModel image_model;
  // Motion:
  super_resolution::MotionShiftSequence motion_shift_sequence;
  motion_shift_sequence.SetMotionSequence({
    super_resolution::MotionShift(0, 0),
    super_resolution::MotionShift(1, 1)
  });
  std::unique_ptr<super_resolution::DegradationOperator> motion_module(
      new super_resolution::MotionModule(motion_shift_sequence));
  image_model.AddDegradationOperator(std::move(motion_module));
  // Downsampling:
  std::unique_ptr<super_resolution::DegradationOperator> downsampling_module(
      new super_resolution::DownsamplingModule(2));
  image_model.AddDegradationOperator(std::move(downsampling_module));

  // Create the solver for the model and low-res images.
  super_resolution::MapSolver solver(image_model, low_res_images);

  // Create the high-res initial estimate.
  const cv::Mat initial_estimate_matrix = (cv::Mat_<double>(4, 4)
      << 0.5, 0.5, 0.5, 0.5,
         0.5, 0.5, 0.5, 0.5,
         0.5, 0.5, 0.5, 0.5,
         0.5, 0.5, 0.5, 0.5);
  super_resolution::ImageData initial_estimate(initial_estimate_matrix);

  super_resolution::ImageData result = solver.Solve(initial_estimate);
  std::cout << result.GetChannelImage(0) << std::endl;
}