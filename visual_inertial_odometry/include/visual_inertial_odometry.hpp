#ifndef VISUAL_INERTIAL_ODOMETRY_
#define VISUAL_INERTIAL_ODOMETRY_

#include <iostream>

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

#include <opencv2/opencv.hpp>

#include "camera_model.hpp"
#include "feature_tracker.hpp"
#include "feature_tracks.hpp"
#include "keyframe.hpp"
#include "mapdata_types.hpp"
#include "map_initializer.hpp"
#include "pnp_estimator.hpp"

#include "vio_data_buffer.hpp"

#ifdef OPENCV_VIZ_FOUND
#include "scene.hpp"
#include "scene_visualizer.hpp"
#endif

namespace vio {

class VisualInertialOdometry {
 public:
  enum VIOStatus {
    UNINITED = 0,
    INITED,
  };

  VisualInertialOdometry(CameraModelPtr camera);

  void Start() {
    // TODO: Is this really necessary?
    running_process_buffer_thread_ = true;
    process_buffer_thread_ = std::unique_ptr<std::thread>(
        new std::thread(&VisualInertialOdometry::ProcessDataInBuffer, this));
  }

  void Stop() {
    // Send stop signal.
    running_process_buffer_thread_ = false;
    data_buffer_.CloseBuffer();

    {
      // std::unique_lock<std::mutex>
      // tmp_lock(running_initializer_thread_mutex_);
      // Check if it's too quick that the initializer hasn't started yet.
      if (initializer_thread_ != nullptr && initializer_thread_->joinable())
        initializer_thread_->join();
    }
    if (process_buffer_thread_ != nullptr && process_buffer_thread_->joinable())
      process_buffer_thread_->join();

    if (running_initializer_flag_.valid()) running_initializer_flag_.wait();

    std::cout << "Image buffer stats:\n";
    data_buffer_.image_buffer_stats().Print();
    GetLandmarkStats(landmarks_, landmark_stats_);
    landmark_stats_.Print();
  }

  /*
   * Called from outside thread contains this class.
   */
  bool ProcessNewImage(cv::Mat &img);
  // TODO: High priority, should update ASAP.
  void ProcessImuData();

// TODO: This should be outside of VIO, but put here for now for easy debugging.
#ifdef OPENCV_VIZ_FOUND
  void VisualizeCurrentScene();
#endif

  void set_single_thread_mode(bool flag) {
    single_thread_mode_ = flag;
    data_buffer_.set_block_when_buffer_full(true);
  }

 private:
  void InitializeFeatureTracker();
  void InitializeVIOInitializer();

  /* ---------------------------------------
   *
   * Main thread. Keep running until stopped.
   *
   */
  void ProcessDataInBuffer();

  // Return true if new keyframe is added.
  bool AddNewKeyframeFromImage(const cv::Mat &new_image);

  void RemoveShortTracksNotVisibleToCurrentKeyframe(
      const KeyframeId &cur_keyframe_id);
  /* ---------------------------------------
   *
   * Thread to initialize first frames and landmarks.
   */
  void RunInitializer(
      const std::vector<KeyframeId> &frame_ids,
      const std::vector<std::vector<cv::Vec2d>> &feature_vectors);

  // When initialization succeeded, copy data to keyframes and landmarks.
  void CopyInitializedFramesAndLandmarksData(
      const std::vector<KeyframeId> &frame_ids,
      const std::vector<cv::Mat> &Rs_est, const std::vector<cv::Mat> &ts_est);

  // After first several keyframes are initialized, propagate to all store
  // keyframes.
  void PropagateInitializationToAllKeyframes();

  // Triangulate landmarks that are visible two or more initialized keyframes.
  bool TriangulteLandmarksInNewKeyframes(Keyframe &pre_frame,
                                         Keyframe &new_frame);

  bool InitializePoseForNewKeyframe(Keyframe &pre_frame, Keyframe &new_frame);

  // Remove a keyframe and associated landmarks if not observed by other frames.
  bool RemoveKeyframe(KeyframeId frame_id);

  /* ---------------------------------------
   *
   * Member variables.
   *
   */
  std::atomic<VIOStatus> vio_status_;

  /*
   *  Functional objects.
   */
  CameraModelPtr camera_;

  FeatureTrackerPtr feature_tracker_;

  MapInitializerPtr map_initializer_;

  PnPEstimatorPtr pnp_estimator_;

  // There must be only one initializer. Because once it fails, it will remove
  // the tried keyframes.
  // TODO: Looks like the initializer shouldn't be start in ProcessBuffer, it
  // should be a separate thread keep running and try to initialize a sliding
  // window of keyframes.
  std::unique_ptr<std::thread> initializer_thread_;
  // TODO: Use future and set_at_thread_end.
  std::atomic<bool> running_initializer_thread_;
  std::future<void> running_initializer_flag_;

  /*
   * Data structures.
   */
  VIODataBuffer data_buffer_;

  std::mutex keyframes_mutex_;
  Keyframes keyframes_;
  Keyframe *last_keyframe_;
  int num_skipped_frames_;

  std::mutex landmarks_mutex_;
  Landmarks landmarks_;
  // [0] is empty, [1] is length 1, [2] is length 2...
  std::vector<std::unordered_set<LandmarkId>> track_length_to_landmark_;
  LandmarkStats landmark_stats_;

  std::unique_ptr<std::thread> process_buffer_thread_;
  std::atomic<bool> running_process_buffer_thread_;

  // Single Thread mode
  // Only one thread can have this mutex at the same.
  std::mutex vio_mutex_;
  bool single_thread_mode_;
};

void RemoveUnmatchedFeatures(Keyframe &frame);

bool ShouldSkipThisFrame(Keyframe *frame0, Keyframe *frame1,
                         const std::vector<cv::DMatch> &matches);

bool ProcessMatchesAndAddToLandmarks(
    Keyframe *frame0, Keyframe *frame1, const std::vector<cv::DMatch> &matches,
    std::vector<std::unordered_set<LandmarkId>> &track_length_to_landmark,
    Landmarks &landmarks);

/*
 * Output:
 *   frame_ids : id of the Keyframes
 *   feature_vectors : features of the Keyframes.
 */
bool CopyDataForInitializer(
    const Landmarks &landmarks, const Keyframes &keyframes,
    std::vector<KeyframeId> &frame_ids,
    std::vector<std::vector<cv::Vec2d>> &feature_vectors);

}  // vio

#endif  // VISUAL_ODOMETRY_
