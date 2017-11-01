#ifndef VISUAL_INERTIAL_ODOMETRY_
#define VISUAL_INERTIAL_ODOMETRY_

#include <iostream>

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
    std::unique_lock<std::mutex> tmp_lock(running_process_buffer_thread_mutex_);
    running_process_buffer_thread_ = true;
    tmp_lock.unlock();
    process_buffer_thread_ = std::unique_ptr<std::thread>(
        new std::thread(&VisualInertialOdometry::ProcessDataInBuffer, this));
  }

  void Stop() {
    // Send stop signal.
    {
      std::unique_lock<std::mutex> tmp_lock(
          running_process_buffer_thread_mutex_);
      running_process_buffer_thread_ = false;
    }

    data_buffer_.CloseBuffer();

    {
      std::unique_lock<std::mutex> tmp_lock(running_initializer_thread_mutex_);
      // Check if it's too quick that the initializer hasn't started yet.
      if (initializer_thread_ != nullptr && initializer_thread_->joinable())
        initializer_thread_->join();
    }
    if (process_buffer_thread_ != nullptr && process_buffer_thread_->joinable())
      process_buffer_thread_->join();

    std::cout << "Image buffer stats:\n";
    data_buffer_.image_buffer_stats().Print();
    GetLandmarkStats(landmarks_, landmark_stats_);
    landmark_stats_.Print();
  }

  /*
   * Called from outside thread contains this class.
   */
  void ProcessNewImage(cv::Mat &img);
  // TODO: High priority, should update ASAP.
  void ProcessImuData();


// TODO: This should be outside of VIO, but put here for now for easy debugging.
#ifdef OPENCV_VIZ_FOUND
  void VisualizeCurrentScene();
#endif

 private:
  void InitializeFeatureTracker();
  void InitializeVIOInitializer();

  /*
   * Main thread. Keep running until stopped.
   */
  void ProcessDataInBuffer();

  // ---------------------------------------

  /*
   * Thread to initialize first frames and landmarks.
   */
  void RunInitializer(
      const std::vector<KeyframeId> &frame_ids,
      const std::vector<std::vector<cv::Vec2d> > &feature_vectors);

  // When initialization succeeded, copy data to keyframes and landmarks.
  void CopyInitializedFramesAndLandmarksData(
      const std::vector<KeyframeId> &frame_ids,
      const std::vector<cv::Mat> &Rs_est, const std::vector<cv::Mat> &ts_est);

  // ---------------------------------------

  std::mutex vio_status_mutex_;
  // TODO: atomic?
  VIOStatus vio_status_;

  /*
   *  Functional objects.
   */
  CameraModelPtr camera_;

  FeatureTrackerPtr feature_tracker_;

  MapInitializerPtr map_initializer_;
  std::unique_ptr<std::thread> initializer_thread_;
  std::mutex running_initializer_thread_mutex_;
  // TODO: Use atomic?
  bool running_initializer_thread_;

  /*
   * Data structures.
   */
  VIODataBuffer data_buffer_;
  std::mutex end_of_buffer_mutex_;
  // TODO: atomic?
  bool end_of_buffer_;

  std::mutex keyframes_mutex_;
  Keyframes keyframes_;
  Keyframe *last_keyframe_;
  int num_skipped_frames_;

  std::mutex landmarks_mutex_;
  Landmarks landmarks_;
  LandmarkStats landmark_stats_;

  std::unique_ptr<std::thread> process_buffer_thread_;
  std::mutex running_process_buffer_thread_mutex_;
  // TODO: atomic? also consider use future / async
  // e.g. : auto f3 = ...
  //        f3.wait_for(timeout) == std::future_status::ready
  bool running_process_buffer_thread_;
};

void RemoveUnmatchedFeatures(Keyframe *frame);

bool ProcessMatchesToLandmarks(Keyframe *frame0, Keyframe *frame1,
                               const std::vector<cv::DMatch> &matches,
                               Landmarks &landmarks);

void RemoveShortTrackLengthLandmark(LandmarkId landmark_id,
                                    Landmarks &landmarks, Keyframes &keyframes);

void RemoveShortTracks(Landmarks &landmarks, KeyframeId &cur_keyframe_id);

/*
 * Output:
 *   frame_ids : id of the Keyframes
 *   feature_vectors : features of the Keyframes.
 */
void CopyDataForInitializer(
    const Landmarks &landmarks, const Keyframes &keyframes,
    std::vector<KeyframeId> &frame_ids,
    std::vector<std::vector<cv::Vec2d> > &feature_vectors);

}  // vio

#endif  // VISUAL_ODOMETRY_
