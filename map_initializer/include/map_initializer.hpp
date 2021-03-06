#include <memory>
#include <vector>

#include <opencv2/opencv.hpp>

#include "multiview.hpp"

namespace vio {

// TODO: Implement each of them.

struct MapInitializerOptions {
  MapInitializerOptions()
      : method(NORMALIZED8POINTFUNDAMENTAL),
        use_f_ransac(true),
        f_ransac_confidence(0.99),
        f_ransac_max_dist_to_epipolar(2),
        reprojection_error_thres(5),
        parallax_thresh(0.9998),
        verbose(false) {}

  enum MapInitializerMethod {
    LIBMV,
    // PTAM,
    // ORBSLAM_F_OR_H,
    // FIVEPOINT,
    // FIVEPOINTEASY,
    NORMALIZED8POINTFUNDAMENTAL
  };

  MapInitializerMethod method;

  // --------- For NORMALIZED8POINTFUNDAMENTAL method
  // Compute Fundamental
  bool use_f_ransac;
  double f_ransac_confidence;
  double f_ransac_max_dist_to_epipolar;

  // triangulation
  double reprojection_error_thres;
  double parallax_thresh;
  // ------------------------------------------------

  bool verbose;

  void read(const cv::FileNode &node) {
    method = static_cast<MapInitializerMethod>((int)node["Method"]);

    use_f_ransac = (int)node["F_USE_RANSAC"];
    f_ransac_confidence = (double)node["F_RANSAC_CONFIDENCE"];
    f_ransac_max_dist_to_epipolar = (double)node["F_RANSAC_MAX_DIST"];
    reprojection_error_thres = (double)node["Reprojection_Error_Threshold"];
    parallax_thresh = (double)node["Parallax_Threshold"];

    verbose = (int)node["VERBOSE"];
  }
};

// Following must be defined for the serialization in FileStorage to work
static void read(
    const cv::FileNode &node, MapInitializerOptions &x,
    const MapInitializerOptions &default_value = MapInitializerOptions()) {
  if (node.empty())
    x = default_value;
  else
    x.read(node);
}

class MapInitializer {
 public:
  MapInitializer() {}
  ~MapInitializer() {}

  static std::unique_ptr<MapInitializer> CreateMapInitializer(
      MapInitializerOptions option);

#ifdef SFM_FOUND
  static std::unique_ptr<MapInitializer> CreateMapInitializerLIBMV();
#endif
  static std::unique_ptr<MapInitializer> CreateMapInitializer8Point(
      MapInitializerOptions option);
  static std::unique_ptr<MapInitializer> CreateMapInitializerORBSLAM(
      MapInitializerOptions option);

  virtual bool Initialize(
      const std::vector<std::vector<cv::Vec2d> > &feature_vectors,
      const cv::Mat &K, std::vector<cv::Point3f> &points3d,
      std::vector<bool> &points3d_mask, std::vector<cv::Mat> &Rs,
      std::vector<cv::Mat> &ts) = 0;

 protected:
};

typedef std::unique_ptr<MapInitializer> MapInitializerPtr;

}  // vio
