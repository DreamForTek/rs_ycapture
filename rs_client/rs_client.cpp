// rsclient.cpp : console app for dummy camera driver
//

#include "stdafx.h"
#include <CaptureSender.h>
#include <ycapture.h>

#include <librealsense2/rs.hpp>
#include <opencv2/opencv.hpp>
#include <Windows.h>

// Resolution will be changed by command line option
int _tmain(int argc, _TCHAR* argv[])
{
	// RealSense initialization
	auto image_width = 848;
	auto image_height = 480;
	auto frame_rate = 30;

	auto min_depth = 0.29f;
	auto max_depth = 10.0f;

	auto exposure_time = 0;
	auto gain = 16;

	auto emitter_power = 0.0f;

	if (argc >= 3)
	{
		image_width = atoi(argv[1]);
		image_height = atoi(argv[2]);
	}

	if (argc >= 5)
	{
		min_depth = atof(argv[3]);
		max_depth = atof(argv[4]);
	}

	if (argc >= 7)
	{
		exposure_time = static_cast<int>(atof(argv[5]) * 1000);
		gain = atoi(argv[6]);
	}

	if (argc >= 8)
	{
		emitter_power = atoi(argv[7]);
	}

	// Declare RealSense pipeline, encapsulating the actual device and sensors
	rs2::pipeline pipe;
	rs2::config cfg;
	// Use a configuration object to request only depth from the pipeline
	cfg.enable_stream(RS2_STREAM_DEPTH, image_width, image_height, RS2_FORMAT_Z16, frame_rate);
	cfg.enable_stream(RS2_STREAM_COLOR, image_width, image_height, RS2_FORMAT_RGB8, frame_rate);
	// Start streaming with the above configuration
	pipe.start(cfg);

	rs2::sensor ir_sensor;
	auto active_sensors = pipe.get_active_profile().get_device().query_sensors();
	for (auto target_sensor : active_sensors)
	{
		if (strcmp(target_sensor.get_info(rs2_camera_info::RS2_CAMERA_INFO_NAME), "Stereo Module") == 0) { ir_sensor = target_sensor; }
	}

	// stereo camera settings
	ir_sensor.set_option(rs2_option::RS2_OPTION_GAIN, gain);
	ir_sensor.set_option(rs2_option::RS2_OPTION_LASER_POWER, emitter_power);
	if (exposure_time > 0)
	{
		ir_sensor.set_option(rs2_option::RS2_OPTION_ENABLE_AUTO_EXPOSURE, 0);
		ir_sensor.set_option(rs2_option::RS2_OPTION_EXPOSURE, exposure_time);
	}
	else
	{
		ir_sensor.set_option(rs2_option::RS2_OPTION_ENABLE_AUTO_EXPOSURE, 1);
	}

	// Declare filters
	rs2::threshold_filter thr_filter;   // Threshold  - removes values outside recommended range
	rs2::disparity_transform depth_to_disparity(true);
	rs2::spatial_filter spat_filter;    // Spatial    - edge-preserving spatial smoothing
	rs2::temporal_filter temp_filter;   // Temporal   - reduces temporal noise
//	rs2::disparity_transform disparity_to_depth(false);
	rs2::colorizer color_filter;
	rs2::align align_to_color(RS2_STREAM_COLOR);

	// filter settings
	thr_filter.set_option(RS2_OPTION_MIN_DISTANCE, min_depth);
	thr_filter.set_option(RS2_OPTION_MAX_DISTANCE, max_depth);
	spat_filter.set_option(RS2_OPTION_FILTER_MAGNITUDE, 2.0f);
	spat_filter.set_option(RS2_OPTION_FILTER_SMOOTH_ALPHA, 0.5f);
	spat_filter.set_option(RS2_OPTION_FILTER_SMOOTH_DELTA, 20.0f);
	temp_filter.set_option(RS2_OPTION_FILTER_SMOOTH_ALPHA, 0.4f);

	// color filter setting
	color_filter.set_option(RS2_OPTION_HISTOGRAM_EQUALIZATION_ENABLED, 0);
	color_filter.set_option(RS2_OPTION_COLOR_SCHEME, 9.0f);
	color_filter.set_option(RS2_OPTION_MIN_DISTANCE, min_depth);
	color_filter.set_option(RS2_OPTION_MAX_DISTANCE, max_depth);

	// Declaring two concurrent queues that will be used to push and pop frames from different threads
	rs2::frame_queue original_data;
	rs2::frame_queue filtered_data;

	// initialize objects
	CaptureSender sender(CS_SHARED_PATH, CS_EVENT_WRITE, CS_EVENT_READ);
	DWORD avgTimePF = 1000 / 15;		// 15fps

	auto image_mat = cv::Mat(cv::Size(image_width, image_height * 2), CV_8UC3);
	auto send_data = new unsigned char[image_width * image_height * 2 * 3];
	auto display_mat = cv::Mat(cv::Size(image_width, image_height * 2), CV_8UC3);
	auto depth_mat = cv::Mat(cv::Size(image_width, image_height), CV_16U);

	auto i = 0; // frame counter
/*
	// remove frame
	cv::namedWindow("RealSense image", 0);
//	cv::setWindowProperty("RealSense image", CV_WND_PROP_FULLSCREEN, CV_WINDOW_KEEPRATIO);
	HWND hwnd = FindWindow(0, "RealSense image");
	SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP); //WS_THICKFRAME or WS_POPUP 
	SetWindowPos(hwnd, NULL, 100, 100, image_width, image_height * 2, SWP_DRAWFRAME | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
	SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG)CreateSolidBrush(RGB(0, 0, 0)));
*/
	auto is_record = false;
	auto video_writer = cv::VideoWriter("capture.mp4", cv::VideoWriter::fourcc('H', '2', '6', '4'), 30, cv::Size(image_width, image_height * 2));

	while (true)
	{
		auto current_frame = pipe.wait_for_frames(); // Wait for next set of frames from the camera
		if (!current_frame) { break; } // Should not happen but if the pipeline is configured differently

		current_frame = align_to_color.process(current_frame);

		rs2::video_frame depth_frame = current_frame.get_depth_frame(); //Take the depth frame from the frameset

		// apply post processing filters
		depth_frame = thr_filter.process(depth_frame);
		depth_frame = depth_to_disparity.process(depth_frame);
		depth_frame = spat_filter.process(depth_frame);
		//		depth_frame = temp_filter.process(depth_frame);
		depth_frame = color_filter.process(depth_frame);

		rs2::video_frame color_frame = current_frame.get_color_frame(); //Take the color frame from the frameset

		memcpy(image_mat.data, color_frame.get_data(), sizeof(unsigned char) * image_width * image_height * 3);
		memcpy(&(image_mat.data[image_width * image_height * 3]), depth_frame.get_data(), sizeof(unsigned char) * image_width * image_height * 3);

		cv::cvtColor(image_mat, display_mat, cv::COLOR_BGR2RGB);
		cv::imshow("RealSense image", display_mat);

		// upside down flip
		for (int y = 0; y < image_height * 2; y++) {
			memcpy(&send_data[image_width * y * 3], &image_mat.data[image_width * (image_height * 2 - y - 1) * 3], sizeof(unsigned char) * image_width * 3);
		}

		auto in_key = cv::waitKey(1);
		if (in_key == 27) { break; } // ESC to escape
		if (in_key == 's') { is_record = !is_record; }

		if (is_record) {
			video_writer << display_mat;
		}

		// Transmit data
		HRESULT hr = sender.Send(i * avgTimePF, image_width, image_height * 2, send_data);
		if (FAILED(hr)) {
			// If error happened, break
			break;
		}
		else if (hr == S_OK) {
			std::cout << "Sent: " << i << std::endl;
		}
		else {
			std::cout << "Ignored: " << i << std::endl;
		}

		i++;
	}

	// Finalize
	pipe.stop();
	delete[] send_data;
	return 0;
}

