#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

namespace {

struct Args {
    int camera_index = 0;
    int camera_width = 0;
    int camera_height = 0;
    double camera_fps = 0.0;
    std::filesystem::path output_dir = "materials/calibration_images";
    std::string prefix = "calib_";
    std::string extension = ".png";
};

void printUsage() {
    std::cerr
        << "Usage:\n"
        << "  capture_calibration_images_tool [--camera <index>]\n"
        << "                                  [--camera-width px --camera-height px]\n"
        << "                                  [--camera-fps value]\n"
        << "                                  [--output-dir materials/calibration_images]\n"
        << "                                  [--prefix calib_]\n"
        << "                                  [--extension png|jpg|jpeg|bmp]\n"
        << "Keys: SPACE/s save raw frame, q/ESC quit.\n";
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool normalizeExtension(std::string& extension) {
    extension = toLower(extension);
    if (extension.empty()) {
        return false;
    }
    if (extension.front() != '.') {
        extension.insert(extension.begin(), '.');
    }
    return extension == ".png" || extension == ".jpg" ||
           extension == ".jpeg" || extension == ".bmp";
}

bool parseArgs(int argc, char** argv, Args& args) {
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--camera" && i + 1 < argc) {
            args.camera_index = std::stoi(argv[++i]);
        } else if (key == "--camera-width" && i + 1 < argc) {
            args.camera_width = std::stoi(argv[++i]);
        } else if (key == "--camera-height" && i + 1 < argc) {
            args.camera_height = std::stoi(argv[++i]);
        } else if (key == "--camera-fps" && i + 1 < argc) {
            args.camera_fps = std::stod(argv[++i]);
        } else if (key == "--output-dir" && i + 1 < argc) {
            args.output_dir = argv[++i];
        } else if (key == "--prefix" && i + 1 < argc) {
            args.prefix = argv[++i];
        } else if (key == "--extension" && i + 1 < argc) {
            args.extension = argv[++i];
        } else {
            std::cerr << "Unknown or incomplete argument: " << key << "\n";
            printUsage();
            return false;
        }
    }

    if (args.camera_index < 0) {
        std::cerr << "--camera must be greater than or equal to 0\n";
        return false;
    }
    if ((args.camera_width > 0) != (args.camera_height > 0)) {
        std::cerr << "--camera-width and --camera-height must be set together\n";
        return false;
    }
    if (args.camera_width < 0 || args.camera_height < 0) {
        std::cerr << "camera width and height must not be negative\n";
        return false;
    }
    if (args.camera_fps < 0.0) {
        std::cerr << "--camera-fps must not be negative\n";
        return false;
    }
    if (args.prefix.empty()) {
        std::cerr << "--prefix must not be empty\n";
        return false;
    }
    if (!normalizeExtension(args.extension)) {
        std::cerr << "--extension must be one of png, jpg, jpeg, bmp\n";
        return false;
    }
    return true;
}

std::filesystem::path buildImagePath(const Args& args, int index) {
    std::ostringstream name;
    name << args.prefix << std::setw(3) << std::setfill('0') << index << args.extension;
    return args.output_dir / name.str();
}

int findNextImageIndex(const Args& args) {
    int index = 1;
    while (std::filesystem::exists(buildImagePath(args, index))) {
        ++index;
    }
    return index;
}

void drawPreviewOverlay(cv::Mat& preview,
                        int next_index,
                        int saved_count,
                        const std::filesystem::path& output_dir) {
    const std::string line1 = "SPACE/S save raw frame, Q/ESC quit";
    std::ostringstream line2;
    line2 << "next=" << next_index
          << " saved=" << saved_count
          << " size=" << preview.cols << "x" << preview.rows;
    const std::string line3 = output_dir.string();

    const cv::Point origin1(16, 32);
    const cv::Point origin2(16, 62);
    const cv::Point origin3(16, 92);
    for (const auto& item : {std::pair<std::string, cv::Point>{line1, origin1},
                             {line2.str(), origin2},
                             {line3, origin3}}) {
        cv::putText(preview, item.first, item.second, cv::FONT_HERSHEY_SIMPLEX,
                    0.7, cv::Scalar(0, 0, 0), 4, cv::LINE_AA);
        cv::putText(preview, item.first, item.second, cv::FONT_HERSHEY_SIMPLEX,
                    0.7, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    }
}

}  // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--help" || key == "-h") {
            printUsage();
            return EXIT_SUCCESS;
        }
    }

    Args args;
    try {
        if (!parseArgs(argc, argv, args)) {
            return EXIT_FAILURE;
        }
        std::filesystem::create_directories(args.output_dir);

        cv::VideoCapture capture;
        if (!capture.open(args.camera_index, cv::CAP_ANY)) {
            std::cerr << "failed to open USB camera: " << args.camera_index << "\n";
            return EXIT_FAILURE;
        }
        if (args.camera_width > 0 && args.camera_height > 0) {
            capture.set(cv::CAP_PROP_FRAME_WIDTH, args.camera_width);
            capture.set(cv::CAP_PROP_FRAME_HEIGHT, args.camera_height);
        }
        if (args.camera_fps > 0.0) {
            capture.set(cv::CAP_PROP_FPS, args.camera_fps);
        }

        std::cout << "camera " << args.camera_index << " opened: "
                  << static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH)) << "x"
                  << static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT))
                  << " fps=" << capture.get(cv::CAP_PROP_FPS) << "\n"
                  << "output_dir=" << args.output_dir.string() << "\n";

        int next_index = findNextImageIndex(args);
        int saved_count = 0;
        cv::namedWindow("honta calibration capture", cv::WINDOW_NORMAL);

        while (true) {
            cv::Mat frame;
            if (!capture.read(frame) || frame.empty()) {
                std::cerr << "read failed from USB camera: " << args.camera_index << "\n";
                continue;
            }

            cv::Mat preview = frame.clone();
            drawPreviewOverlay(preview, next_index, saved_count, args.output_dir);
            cv::imshow("honta calibration capture", preview);

            const int key = cv::waitKey(1);
            if (key == 27 || key == 'q' || key == 'Q') {
                break;
            }
            if (key == ' ' || key == 's' || key == 'S') {
                const std::filesystem::path output_path = buildImagePath(args, next_index);
                if (!cv::imwrite(output_path.string(), frame)) {
                    std::cerr << "failed to save " << output_path.string() << "\n";
                    continue;
                }
                std::cout << "saved " << output_path.string() << "\n";
                ++saved_count;
                do {
                    ++next_index;
                } while (std::filesystem::exists(buildImagePath(args, next_index)));
            }
        }

        std::cout << "capture complete, saved " << saved_count << " image(s)\n";
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
