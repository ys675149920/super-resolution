// The super-resolution cost function (defined for Ceres) for the maximum a
// posteriori formulation.

#ifndef SRC_SOLVERS_MAP_COST_FUNCTION_H_
#define SRC_SOLVERS_MAP_COST_FUNCTION_H_

#include <vector>

#include "image/image_data.h"
#include "solvers/map_cost_processor.h"

#include "ceres/ceres.h"

namespace super_resolution {

struct MapCostFunction {
  MapCostFunction(
      const int image_index,
      const int channel_index,
      const int num_pixels,
      const MapCostProcessor& map_cost_processor)
  : image_index_(image_index),
    channel_index_(channel_index),
    num_pixels_(num_pixels),
    map_cost_processor_(map_cost_processor) {}

  template <typename T>
  bool operator() (const T* const high_res_image_estimate, T* residuals) const {
    // TODO: maybe we only need to do this once per solver iteration? Check.
    std::vector<double> pixel_values;
    pixel_values.reserve(num_pixels_);
    for (int i = 0; i < num_pixels_; ++i) {
      // TODO: it's a bit annoying to convert it to the same Jet type explicity
      // before getting out the double, but it won't compile otherwise.
      pixel_values.push_back(
          ceres::Jet<double, 1>(high_res_image_estimate[i]).a);
    }
    residuals[0] = T(0.0);
    return true;
  }

  static ceres::CostFunction* Create(
      const int image_index,
      const int channel_index,
      const int num_pixels,
      const MapCostProcessor& map_cost_processor) {
    // The cost function takes one value - a pixel intensity - and returns the
    // residual between that pixel intensity and the expected observation.
    return (new ceres::AutoDiffCostFunction<MapCostFunction, 1, 1>(
        new MapCostFunction(
            image_index, channel_index, num_pixels, map_cost_processor)));
  }

  const int image_index_;
  const int channel_index_;
  const int num_pixels_;
  const MapCostProcessor& map_cost_processor_;
};

}  // namespace super_resolution

#endif  // SRC_SOLVERS_MAP_COST_FUNCTION_H_
