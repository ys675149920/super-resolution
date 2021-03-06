// Contains code for the shift-add fusion algorithm as explained in "An
// Introduction to Super-Resolution Imaging (2012)".

#include <string>
#include <vector>

#include "motion/motion_shift.h"
#include "util/macros.h"
#include "util/util.h"
#include "video/video_loader.h"

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/photo/photo.hpp"

#include "gflags/gflags.h"
#include "glog/logging.h"

// Input of the LR files and the motion estimates.
DEFINE_string(input_image_dir, "",
    "Path to a directory containing the LR images in alphabetical order.");
DEFINE_string(input_motion_sequence, "",
    "Path to a text file containing a simulated motion sequence.");

// Parameters for generating the high-resolution image.
DEFINE_int32(upsampling_scale, 2,
    "The scale by which to up-scale the LR images.");

int main(int argc, char** argv) {
  super_resolution::util::InitApp(argc, argv,
      "A trivial implementation of shift-add fusion.");

  REQUIRE_ARG(FLAGS_input_image_dir);
  REQUIRE_ARG(FLAGS_input_motion_sequence);

  super_resolution::VideoLoader video_loader;
  video_loader.LoadFramesFromDirectory(FLAGS_input_image_dir);

  // Create an empty HR image.
  const cv::Size low_res_image_size = video_loader.GetImageSize();
  const int fused_width = FLAGS_upsampling_scale * low_res_image_size.width;
  const int fused_height = FLAGS_upsampling_scale * low_res_image_size.height;
  cv::Mat fusion_image = cv::Mat::zeros(fused_width, fused_height, CV_8UC1);

  // Non-zero pixels in the inpaint mask will indicate where the SR image needs
  // to be inpainted after fusion.
  cv::Mat inpaint_mask = cv::Mat::ones(fused_width, fused_height, CV_8UC1);

  // TODO: Eventually estimate the motion automatically.
  super_resolution::MotionShiftSequence motion_shift_sequence;
  motion_shift_sequence.LoadSequenceFromFile(FLAGS_input_motion_sequence);

  const std::vector<cv::Mat>& frames = video_loader.GetFrames();
  CHECK(motion_shift_sequence.GetNumMotionShifts() == frames.size())
      << "The number of motion estimates must match the number of frames.";

  for (int i = 0; i < frames.size(); ++i) {
    cv::Mat frame = frames[i];
    cv::cvtColor(frame, frame, CV_BGR2GRAY);

    // Add this frame to the SR image.
    for (int x = 0; x < frame.cols; ++x) {
      for (int y = 0; y < frame.rows; ++y) {
        const super_resolution::MotionShift& motion_shift =
            motion_shift_sequence[i];
        const int hr_x = FLAGS_upsampling_scale * x - motion_shift.dx;
        const int hr_y = FLAGS_upsampling_scale * y - motion_shift.dy;
        if (hr_x < 0 || hr_x >= fused_width ||
            hr_y < 0 || hr_y >= fused_height) {
          continue;
        }
        fusion_image.at<uchar>(hr_y, hr_x) = frame.at<uchar>(y, x);
        inpaint_mask.at<uchar>(hr_y, hr_x) = 0;
      }
    }
  }

  // Display the image before inpainting.
  cv::imshow("Visualization", fusion_image);
  cv::waitKey(0);

  // Then inpaint it and display it after.
  cv::Mat inpainted_image;
  cv::inpaint(
      fusion_image,
      inpaint_mask,
      inpainted_image,
      FLAGS_upsampling_scale,  // The radius considered for inpainting.
      cv::INPAINT_NS);
  cv::imshow("Visualization", inpainted_image);
  cv::waitKey(0);

  return EXIT_SUCCESS;
}
