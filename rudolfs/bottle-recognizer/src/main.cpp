#include <cstdlib>
#include <iostream>

#include <opencv2/opencv.hpp>

#define USE_HOUGH_CIRCLES 0
#define USE_CONTOUR_DETECTION 0
#define USE_COLOR_FILTER 1
#define SHOW_DEBUG 0  // Show intermediate processing steps

int main(int argc, char** argv) 
{
    if (argc != 2)
    {
        std::cerr << "usage: " << argv[0] << " <video path>" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string video_path = argv[1];
    cv::VideoCapture video_capture(video_path);
    if (!video_capture.isOpened())
    {
        std::cerr << "failed to open video " << video_path << std::endl;
        return EXIT_FAILURE;
    }

    std::unique_ptr<cv::VideoWriter> video_writer;

    size_t i = 0;
    while (true)
    {
        cv::Mat frame;
        video_capture >> frame;
        if (frame.empty())
        {
            break;
        }
        
        if (!video_writer)
        {
            //const auto codec = cv::VideoWriter::fourcc('h', 'e', 'v', '1');
            const auto codec = cv::VideoWriter::fourcc('a', 'v', 'c', '1');
            video_writer = std::make_unique<cv::VideoWriter>("output.mp4", codec, 30, cv::Size(frame.cols, frame.rows));
            if (!video_writer->isOpened())
            {
                std::cerr << "failed to open video writer" << std::endl;
                return EXIT_FAILURE;
            }
        }

        cv::Mat display = frame.clone();
        size_t bottle_count = 0;

#if defined(USE_COLOR_FILTER) && USE_COLOR_FILTER == 1
        // ===== Method 3: Color-Based Filtering (Best for known cap colors) =====
        
        // Convert BGR to HSV color space (better for color filtering)
        cv::Mat hsv;
        cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
        
        // Define range for blue color in HSV
        // Hue: 100-130 (blue range in OpenCV's 0-180 scale)
        // Saturation: 50-255 (ignore very desaturated/grayish blues)
        // Value: 50-255 (ignore very dark regions)
        cv::Scalar lower_blue(100, 50, 50);   // Adjust these values to match your bottle cap blue
        cv::Scalar upper_blue(130, 255, 255);
        
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
            
            // Filter by area (adjust based on your bottle cap size in pixels)
            if (area < 40 || area > 50000)
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
        
#if 0
        std::string title = "frame " + std::to_string(i++) + " - Blue Bottles: " + std::to_string(bottle_count);
        cv::imshow(title.c_str(), display);

        int key = cv::waitKey();
        if (key == 27 || key == 'q')  // ESC or 'q' to quit
            break;
#endif

        video_writer->write(display);
    }

    return EXIT_SUCCESS;
}
