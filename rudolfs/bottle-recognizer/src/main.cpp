#include <cstdlib>
#include <iostream>

#include <opencv2/opencv.hpp>

#define USE_HOUGH_CIRCLES 0
#define USE_CONTOUR_DETECTION 0
#define USE_COLOR_FILTER 1
#define SHOW_DEBUG 0  // Show intermediate processing steps

// Global pause state for button callback
int g_pause_video = 0;

void pauseButtonCallback(int state, void* userData)
{
    g_pause_video = state;
    std::cout << (state ? "Video PAUSED - Press SPACE to resume" : "Video PLAYING") << std::endl;
}

int main(int argc, char** argv) 
{
    const std::string initial_video = (argc == 2) ? argv[1] : "";
    
    // Create window for trackbars
    cv::namedWindow("Parameters", cv::WINDOW_NORMAL);
    
    // HSV color range parameters (adjustable with trackbars)
    int lower_hue = 100, lower_sat = 50, lower_val = 50;
    int upper_hue = 130, upper_sat = 255, upper_val = 255;
    int min_area = 40, max_area = 50000;
    int pause_toggle = 0;
    int speed_slider = 10;  // 10 = 1.0x speed (range 1-20 = 0.1x to 2.0x)
    
    cv::createTrackbar("Lower Hue", "Parameters", &lower_hue, 180);
    cv::createTrackbar("Lower Sat", "Parameters", &lower_sat, 255);
    cv::createTrackbar("Lower Val", "Parameters", &lower_val, 255);
    cv::createTrackbar("Upper Hue", "Parameters", &upper_hue, 180);
    cv::createTrackbar("Upper Sat", "Parameters", &upper_sat, 255);
    cv::createTrackbar("Upper Val", "Parameters", &upper_val, 255);
    cv::createTrackbar("Min Area", "Parameters", &min_area, 10000);
    cv::createTrackbar("Max Area", "Parameters", &max_area, 100000);
    cv::createTrackbar("Video Speed (0.1x - 2.0x)", "Parameters", &speed_slider, 20);
    cv::createTrackbar("PAUSE (toggle with SPACE key)", "Parameters", &pause_toggle, 1, pauseButtonCallback);
    
    std::string current_video = initial_video;
    
    while (true)
    {
        if (current_video.empty())
        {
            std::cout << "Enter video file path (or 'quit' to exit): ";
            std::getline(std::cin, current_video);
            if (current_video == "quit" || current_video == "q")
                break;
        }
        
        cv::VideoCapture video_capture(current_video);
        if (!video_capture.isOpened())
        {
            std::cerr << "failed to open video: " << current_video << std::endl;
            current_video.clear();
            continue;
        }
        
        std::cout << "Playing: " << current_video << std::endl;
        std::cout << "Controls:" << std::endl;
        std::cout << "  SPACE - Pause/Resume" << std::endl;
        std::cout << "  'r' - Restart video from beginning" << std::endl;
        std::cout << "  'l' - Load new video file" << std::endl;
        std::cout << "  ESC or 'q' - Quit application" << std::endl;
        
        bool load_new_video = false;
        while (true)
        {
            cv::Mat frame;
            
            // Skip frame if not paused
            if (pause_toggle == 0)
            {
                video_capture >> frame;
                if (frame.empty())
                {
                    std::cout << "Video finished. Load another? (Enter path or 'quit'): ";
                    current_video.clear();
                    break;
                }
            }
            else
            {
                // Stay on same frame when paused
                if (!frame.empty() || frame.empty())
                {
                    // Just refresh display
                }
            }
            
            if (frame.empty() && pause_toggle == 0)
                break;
            
            // If we're paused, keep showing last frame
            static cv::Mat last_frame;
            if (!frame.empty())
                last_frame = frame.clone();
            else if (!last_frame.empty())
                frame = last_frame.clone();
            else
                continue;

            cv::Mat display = frame.clone();
            size_t bottle_count = 0;

#if defined(USE_COLOR_FILTER) && USE_COLOR_FILTER == 1
        // ===== Method 3: Color-Based Filtering (Best for known cap colors) =====
        
        // Convert BGR to HSV color space (better for color filtering)
        cv::Mat hsv;
        cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
        
        // Define range for blue color in HSV (using trackbar values)
        // Hue: 100-130 (blue range in OpenCV's 0-180 scale)
        // Saturation: 50-255 (ignore very desaturated/grayish blues)
        // Value: 50-255 (ignore very dark regions)
        cv::Scalar lower_blue(lower_hue, lower_sat, lower_val);
        cv::Scalar upper_blue(upper_hue, upper_sat, upper_val);
        
        // Create mask for blue regions
        cv::Mat blue_mask;
        cv::inRange(hsv, lower_blue, upper_blue, blue_mask);
        
#if SHOW_DEBUG
        cv::imshow("1. Blue Mask", blue_mask);
#endif
        
        // Apply morphological operations to clean up the mask
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
        cv::Mat cleaned_mask;
        cv::morphologyEx(blue_mask, cleaned_mask, cv::MORPH_CLOSE, kernel);  // Close small gaps
        cv::morphologyEx(cleaned_mask, cleaned_mask, cv::MORPH_OPEN, kernel); // Remove small noise
        
#if SHOW_DEBUG
        cv::imshow("2. Cleaned Mask", cleaned_mask);
#endif
        
        // Find contours in the cleaned mask
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(cleaned_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        // Filter contours by shape properties
        std::vector<std::vector<cv::Point>> bottle_contours;
        for (const auto& contour : contours)
        {
            double area = cv::contourArea(contour);
            
            // Filter by area (using trackbar values)
            if (area < min_area || area > max_area)
                continue;
            
            double perimeter = cv::arcLength(contour, true);
            
            // Calculate circularity: 4π * area / perimeter^2
            // Perfect circle = 1.0, accept 0.5+ for irregular shapes
            double circularity = 4 * CV_PI * area / (perimeter * perimeter);
            
            // Get bounding rectangle to check aspect ratio
            cv::Rect bbox = cv::boundingRect(contour);
            double aspect_ratio = (double)bbox.width / bbox.height;
            
            // Filter by circularity and aspect ratio (should be roundish)
            //if (circularity > 0.5 && aspect_ratio > 0.5 && aspect_ratio < 2.0)
            {
                bottle_contours.push_back(contour);
            }
        }
        
        bottle_count = bottle_contours.size();
        
        // Draw detected blue bottle caps
        for (size_t j = 0; j < bottle_contours.size(); j++)
        {
            // Draw contour in green
            cv::drawContours(display, bottle_contours, j, cv::Scalar(0, 255, 0), 2);
            
            // Calculate and draw center
            cv::Moments m = cv::moments(bottle_contours[j]);
            if (m.m00 != 0)
            {
                cv::Point center(m.m10 / m.m00, m.m01 / m.m00);
                cv::circle(display, center, 5, cv::Scalar(0, 0, 255), -1);
                
                // Add label
                cv::putText(display, std::to_string(j + 1), center + cv::Point(10, 0), 
                           cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 2);
                
                // Draw bounding box for debugging
                cv::Rect bbox = cv::boundingRect(bottle_contours[j]);
                cv::rectangle(display, bbox, cv::Scalar(255, 0, 255), 1);
            }
        }
        
#elif defined(USE_HOUGH_CIRCLES) && USE_HOUGH_CIRCLES == 1
        // ===== Method 1: Hough Circle Transform =====
        
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::Mat blurred;
        cv::GaussianBlur(gray, blurred, cv::Size(9, 9), 2);
        
        std::vector<cv::Vec3f> circles;
        cv::HoughCircles(blurred, circles, cv::HOUGH_GRADIENT, 1,
                        20,              // min distance between circles
                        50, 30,          // Canny threshold and accumulator threshold
                        10, 50);         // min and max radius
        
        bottle_count = circles.size();
        
        for (size_t j = 0; j < circles.size(); j++)
        {
            cv::Point center(cvRound(circles[j][0]), cvRound(circles[j][1]));
            int radius = cvRound(circles[j][2]);
            
            cv::circle(display, center, 3, cv::Scalar(0, 255, 0), -1);
            cv::circle(display, center, radius, cv::Scalar(0, 0, 255), 2);
            cv::putText(display, std::to_string(j + 1), center, 
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 0), 2);
        }
        
#elif defined(USE_CONTOUR_DETECTION) && USE_CONTOUR_DETECTION == 1
        // ===== Method 2: Grayscale Contour Detection =====
        
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        
        cv::Mat binary;
        cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY + cv::THRESH_OTSU);
        
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        bottle_count = contours.size();
        
        for (size_t j = 0; j < contours.size(); j++)
        {
            cv::drawContours(display, contours, j, cv::Scalar(0, 255, 0), 2);
        }
        
#endif
        
        // Display live window with detected bottles
        cv::imshow("Bottle Detection", display);

        // Calculate wait time based on speed slider (0.1x to 2.0x)
        // speed_slider: 1-20, where 10 = 1.0x speed (normal)
        // Normal delay = 30ms, so: delay = 300 / speed_slider
        int delay = std::max(1, 300 / speed_slider);
        
        int key = cv::waitKey(delay);  // Adaptive delay based on speed
        if (key == 27 || key == 'q')  // ESC or 'q' to quit
            return EXIT_SUCCESS;
        else if (key == ' ')  // SPACE to toggle pause
        {
            pause_toggle = 1 - pause_toggle;
            std::cout << (pause_toggle ? "Video PAUSED" : "Video PLAYING") << std::endl;
        }
        else if (key == 'r' || key == 'R')  // 'r' to restart video
        {
            video_capture.set(cv::CAP_PROP_POS_FRAMES, 0);
            std::cout << "Video restarted from beginning" << std::endl;
        }
        else if (key == 'l' || key == 'L')  // 'l' to load new video
        {
            current_video.clear();
            break;
        }
        }  // End of inner while loop
    }  // End of outer while loop

    return EXIT_SUCCESS;
}
