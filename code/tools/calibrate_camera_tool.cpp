#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <nlohmann/json.hpp>

namespace {

struct CropEdges {
    bool enabled = false;
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

struct Args {
    std::filesystem::path images_dir;
    int board_cols = 0;
    int board_rows = 0;
    double square_size_mm = 0.0;
    std::string camera_id = "top_rgb_camera";
    std::filesystem::path crop_config_path;
    std::filesystem::path output_dir = "output/calibration";
    bool save_debug = false;
};

struct CalibrationOutput {
    std::string camera_id;
    int image_width = 0;
    int image_height = 0;
    double fx = 0.0;
    double fy = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    double k1 = 0.0;
    double k2 = 0.0;
    double p1 = 0.0;
    double p2 = 0.0;
    double k3 = 0.0;
};

void printUsage() {
    std::cerr
        << "Usage:\n"
        << "  calibrate_camera_tool --images <dir>\n"
        << "                        --board-cols <inner_corner_cols>\n"
        << "                        --board-rows <inner_corner_rows>\n"
        << "                        --square-size-mm <size>\n"
        << "                        [--camera-id top_rgb_camera]\n"
        << "                        [--crop-config config.json]\n"
        << "                        [--output-dir output/calibration]\n"
        << "                        [--save-debug]\n";
}

bool parseArgs(int argc, char** argv, Args& args) {
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--images" && i + 1 < argc) {
            args.images_dir = argv[++i];
        } else if (key == "--board-cols" && i + 1 < argc) {
            args.board_cols = std::stoi(argv[++i]);
        } else if (key == "--board-rows" && i + 1 < argc) {
            args.board_rows = std::stoi(argv[++i]);
        } else if (key == "--square-size-mm" && i + 1 < argc) {
            args.square_size_mm = std::stod(argv[++i]);
        } else if (key == "--camera-id" && i + 1 < argc) {
            args.camera_id = argv[++i];
        } else if (key == "--crop-config" && i + 1 < argc) {
            args.crop_config_path = argv[++i];
        } else if (key == "--output-dir" && i + 1 < argc) {
            args.output_dir = argv[++i];
        } else if (key == "--save-debug") {
            args.save_debug = true;
        } else {
            std::cerr << "Unknown or incomplete argument: " << key << "\n";
            printUsage();
            return false;
        }
    }

    if (args.images_dir.empty()) {
        std::cerr << "--images is required\n";
        return false;
    }
    if (args.board_cols <= 1 || args.board_rows <= 1) {
        std::cerr << "--board-cols and --board-rows must be inner corner counts greater than 1\n";
        return false;
    }
    if (args.square_size_mm <= 0.0) {
        std::cerr << "--square-size-mm must be greater than 0\n";
        return false;
    }
    if (args.camera_id.empty()) {
        std::cerr << "--camera-id must not be empty\n";
        return false;
    }
    return true;
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool isImageFile(const std::filesystem::path& path) {
    const std::string ext = toLower(path.extension().string());
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp";
}

std::vector<std::filesystem::path> listImageFiles(const std::filesystem::path& images_dir) {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(images_dir)) {
        if (entry.is_regular_file() && isImageFile(entry.path())) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
        return a.filename().string() < b.filename().string();
    });
    return files;
}

std::vector<cv::Point3f> buildBoardObjectPoints(int board_cols,
                                                int board_rows,
                                                double square_size_mm) {
    std::vector<cv::Point3f> points;
    points.reserve(static_cast<std::size_t>(board_cols * board_rows));
    for (int row = 0; row < board_rows; ++row) {
        for (int col = 0; col < board_cols; ++col) {
            points.emplace_back(static_cast<float>(col * square_size_mm),
                                static_cast<float>(row * square_size_mm),
                                0.0f);
        }
    }
    return points;
}

bool findChessboardCornersRobust(const cv::Mat& image,
                                 const cv::Size& board_size,
                                 std::vector<cv::Point2f>& corners) {
    cv::Mat gray;
    if (image.channels() == 1) {
        gray = image;
    } else {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    }

    const int sb_flags = cv::CALIB_CB_NORMALIZE_IMAGE |
                         cv::CALIB_CB_EXHAUSTIVE |
                         cv::CALIB_CB_ACCURACY;
    if (cv::findChessboardCornersSB(gray, board_size, corners, sb_flags)) {
        return true;
    }

    const int classic_flags = cv::CALIB_CB_ADAPTIVE_THRESH |
                              cv::CALIB_CB_NORMALIZE_IMAGE;
    if (!cv::findChessboardCorners(gray, board_size, corners, classic_flags)) {
        return false;
    }

    cv::cornerSubPix(gray,
                     corners,
                     cv::Size(11, 11),
                     cv::Size(-1, -1),
                     cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.001));
    return true;
}

void saveDebugImage(const cv::Mat& image,
                    const cv::Size& board_size,
                    const std::vector<cv::Point2f>& corners,
                    bool found,
                    const std::filesystem::path& output_path) {
    cv::Mat debug;
    if (image.channels() == 1) {
        cv::cvtColor(image, debug, cv::COLOR_GRAY2BGR);
    } else {
        debug = image.clone();
    }

    if (found) {
        cv::drawChessboardCorners(debug, board_size, corners, true);
    } else {
        cv::putText(debug,
                    "chessboard not found",
                    cv::Point(24, 48),
                    cv::FONT_HERSHEY_SIMPLEX,
                    1.0,
                    cv::Scalar(0, 0, 255),
                    2,
                    cv::LINE_AA);
    }
    cv::imwrite(output_path.string(), debug);
}

double coeffAt(const cv::Mat& coeffs, int index) {
    const cv::Mat flat = coeffs.reshape(1, 1);
    if (index < 0 || index >= flat.cols) {
        return 0.0;
    }
    return flat.at<double>(0, index);
}

CalibrationOutput buildOutput(const std::string& camera_id,
                              const cv::Size& image_size,
                              const cv::Mat& camera_matrix,
                              const cv::Mat& dist_coeffs) {
    CalibrationOutput output;
    output.camera_id = camera_id;
    output.image_width = image_size.width;
    output.image_height = image_size.height;
    output.fx = camera_matrix.at<double>(0, 0);
    output.fy = camera_matrix.at<double>(1, 1);
    output.cx = camera_matrix.at<double>(0, 2);
    output.cy = camera_matrix.at<double>(1, 2);
    output.k1 = coeffAt(dist_coeffs, 0);
    output.k2 = coeffAt(dist_coeffs, 1);
    output.p1 = coeffAt(dist_coeffs, 2);
    output.p2 = coeffAt(dist_coeffs, 3);
    output.k3 = coeffAt(dist_coeffs, 4);
    return output;
}

nlohmann::json toCameraJson(const CalibrationOutput& output) {
    return {
        {"camera_id", output.camera_id},
        {"image_width", output.image_width},
        {"image_height", output.image_height},
        {"intrinsic", {
            {"fx", output.fx},
            {"fy", output.fy},
            {"cx", output.cx},
            {"cy", output.cy},
        }},
        {"distortion", {
            {"k1", output.k1},
            {"k2", output.k2},
            {"p1", output.p1},
            {"p2", output.p2},
            {"k3", output.k3},
        }},
        {"mount", {
            {"x_mm", 0.0},
            {"y_mm", 0.0},
            {"z_mm", 0.0},
            {"yaw_deg", 0.0},
            {"pitch_deg", 0.0},
            {"roll_deg", 0.0},
        }},
    };
}

void writeJsonFile(const std::filesystem::path& path, const nlohmann::json& json) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("failed to open output file: " + path.string());
    }
    output << std::setw(2) << json << "\n";
}

template <typename T>
void readIfPresent(const nlohmann::json& object, const char* key, T& target) {
    if (object.contains(key)) {
        target = object.at(key).get<T>();
    }
}

CropEdges loadCropConfig(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open crop config: " + path.string());
    }

    nlohmann::json root;
    input >> root;
    if (!root.contains("crop") || !root.at("crop").is_object()) {
        throw std::runtime_error("crop config must contain a crop object");
    }

    const auto& crop = root.at("crop");
    if (crop.contains("x") || crop.contains("y") ||
        crop.contains("width") || crop.contains("height")) {
        throw std::runtime_error("legacy crop x/y/width/height are not supported; use left/top/right/bottom");
    }

    CropEdges edges;
    readIfPresent(crop, "enabled", edges.enabled);
    readIfPresent(crop, "left", edges.left);
    readIfPresent(crop, "top", edges.top);
    readIfPresent(crop, "right", edges.right);
    readIfPresent(crop, "bottom", edges.bottom);
    if (edges.left < 0 || edges.top < 0 || edges.right < 0 || edges.bottom < 0) {
        throw std::runtime_error("crop left/top/right/bottom must be greater than or equal to 0");
    }
    if (!edges.enabled) {
        edges.left = 0;
        edges.top = 0;
        edges.right = 0;
        edges.bottom = 0;
    }
    return edges;
}

CalibrationOutput cropOutput(const CalibrationOutput& full, const CropEdges& crop) {
    CalibrationOutput cropped = full;
    cropped.image_width = full.image_width - crop.left - crop.right;
    cropped.image_height = full.image_height - crop.top - crop.bottom;
    if (cropped.image_width <= 0 || cropped.image_height <= 0) {
        throw std::runtime_error("crop edges leave no valid image area");
    }
    cropped.cx = full.cx - static_cast<double>(crop.left);
    cropped.cy = full.cy - static_cast<double>(crop.top);
    if (cropped.cx < 0.0 || cropped.cx >= static_cast<double>(cropped.image_width) ||
        cropped.cy < 0.0 || cropped.cy >= static_cast<double>(cropped.image_height)) {
        throw std::runtime_error("cropped principal point is outside cropped image bounds; "
                                 "check crop values and calibration images");
    }
    return cropped;
}

std::vector<double> computePerViewErrors(const std::vector<std::vector<cv::Point3f>>& object_points,
                                         const std::vector<std::vector<cv::Point2f>>& image_points,
                                         const std::vector<cv::Mat>& rvecs,
                                         const std::vector<cv::Mat>& tvecs,
                                         const cv::Mat& camera_matrix,
                                         const cv::Mat& dist_coeffs) {
    std::vector<double> errors;
    errors.reserve(image_points.size());
    for (std::size_t i = 0; i < image_points.size(); ++i) {
        std::vector<cv::Point2f> projected;
        cv::projectPoints(object_points[i], rvecs[i], tvecs[i], camera_matrix, dist_coeffs, projected);
        const double err = cv::norm(image_points[i], projected, cv::NORM_L2);
        errors.push_back(std::sqrt((err * err) / static_cast<double>(projected.size())));
    }
    return errors;
}

}  // namespace

int main(int argc, char** argv) {
    Args args;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string key = argv[i];
            if (key == "--help" || key == "-h") {
                printUsage();
                return EXIT_SUCCESS;
            }
        }
        if (!parseArgs(argc, argv, args)) {
            return EXIT_FAILURE;
        }

        if (!std::filesystem::is_directory(args.images_dir)) {
            std::cerr << "images directory does not exist: " << args.images_dir << "\n";
            return EXIT_FAILURE;
        }
        std::filesystem::create_directories(args.output_dir);

        const cv::Size board_size(args.board_cols, args.board_rows);
        const std::vector<cv::Point3f> board_object_points =
            buildBoardObjectPoints(args.board_cols, args.board_rows, args.square_size_mm);
        const std::vector<std::filesystem::path> image_files = listImageFiles(args.images_dir);
        if (image_files.empty()) {
            std::cerr << "no calibration images found in: " << args.images_dir << "\n";
            return EXIT_FAILURE;
        }

        std::filesystem::path debug_dir;
        if (args.save_debug) {
            debug_dir = args.output_dir / "debug_corners";
            std::filesystem::create_directories(debug_dir);
        }

        std::vector<std::vector<cv::Point3f>> object_points;
        std::vector<std::vector<cv::Point2f>> image_points;
        std::vector<std::string> accepted_names;
        cv::Size image_size;

        std::cout << "scanning " << image_files.size() << " image(s)\n";
        for (const auto& image_path : image_files) {
            const cv::Mat image = cv::imread(image_path.string(), cv::IMREAD_COLOR);
            if (image.empty()) {
                std::cerr << "skip unreadable image: " << image_path << "\n";
                continue;
            }
            if (image_size.width == 0 && image_size.height == 0) {
                image_size = image.size();
            } else if (image.size() != image_size) {
                std::cerr << "skip image with different size: " << image_path
                          << " size=" << image.cols << "x" << image.rows
                          << " expected=" << image_size.width << "x" << image_size.height << "\n";
                continue;
            }

            std::vector<cv::Point2f> corners;
            const bool found = findChessboardCornersRobust(image, board_size, corners);
            if (args.save_debug) {
                const std::string suffix = found ? "_ok.jpg" : "_failed.jpg";
                saveDebugImage(image,
                               board_size,
                               corners,
                               found,
                               debug_dir / (image_path.stem().string() + suffix));
            }

            std::cout << (found ? "[ok]   " : "[fail] ") << image_path.filename().string() << "\n";
            if (!found) {
                continue;
            }
            object_points.push_back(board_object_points);
            image_points.push_back(std::move(corners));
            accepted_names.push_back(image_path.filename().string());
        }

        if (image_points.size() < 3) {
            std::cerr << "need at least 3 successful calibration images, got "
                      << image_points.size() << "\n";
            return EXIT_FAILURE;
        }
        if (image_points.size() < 8) {
            std::cerr << "warning: only " << image_points.size()
                      << " successful images; 8-15 or more is recommended for field calibration\n";
        }

        cv::Mat camera_matrix = cv::Mat::eye(3, 3, CV_64F);
        cv::Mat dist_coeffs = cv::Mat::zeros(5, 1, CV_64F);
        std::vector<cv::Mat> rvecs;
        std::vector<cv::Mat> tvecs;
        const double rms = cv::calibrateCamera(object_points,
                                               image_points,
                                               image_size,
                                               camera_matrix,
                                               dist_coeffs,
                                               rvecs,
                                               tvecs);

        const CalibrationOutput full_output =
            buildOutput(args.camera_id, image_size, camera_matrix, dist_coeffs);
        const std::filesystem::path full_path =
            args.output_dir / (args.camera_id + "_full.json");
        writeJsonFile(full_path, toCameraJson(full_output));

        std::cout << "\ncalibration complete\n"
                  << "rms_reprojection_error=" << std::fixed << std::setprecision(4) << rms << "\n"
                  << "accepted_images=" << image_points.size() << "/" << image_files.size() << "\n"
                  << "image_size=" << image_size.width << "x" << image_size.height << "\n"
                  << "full_output=" << full_path.string() << "\n";

        const std::vector<double> per_view_errors =
            computePerViewErrors(object_points, image_points, rvecs, tvecs, camera_matrix, dist_coeffs);
        for (std::size_t i = 0; i < per_view_errors.size(); ++i) {
            std::cout << "view_error " << accepted_names[i]
                      << "=" << std::fixed << std::setprecision(4)
                      << per_view_errors[i] << "\n";
        }

        if (!args.crop_config_path.empty()) {
            const CropEdges crop = loadCropConfig(args.crop_config_path);
            if (!crop.enabled) {
                std::cout << "crop_config=" << args.crop_config_path.string()
                          << " has crop.enabled=false; cropped output uses full frame\n";
            } else if (crop.left == crop.right && crop.top == crop.bottom) {
                std::cout << "crop is strict center crop: left=right=" << crop.left
                          << " top=bottom=" << crop.top << "\n";
            } else {
                std::cout << "warning: crop is not strict center crop: left=" << crop.left
                          << " top=" << crop.top
                          << " right=" << crop.right
                          << " bottom=" << crop.bottom << "\n";
            }

            const CalibrationOutput cropped_output = cropOutput(full_output, crop);
            const std::filesystem::path cropped_path =
                args.output_dir / (args.camera_id + "_cropped.json");
            writeJsonFile(cropped_path, toCameraJson(cropped_output));
            std::cout << "cropped_output=" << cropped_path.string() << "\n";
        }

        if (args.save_debug) {
            std::cout << "debug_corners=" << debug_dir.string() << "\n";
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
