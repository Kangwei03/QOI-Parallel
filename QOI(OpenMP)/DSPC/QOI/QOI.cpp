//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//#include <time.h>
//#include <Windows.h>
//#include <string>
//#include <chrono>
//#include <iomanip>
//#include <sstream>
//#include <omp.h>
//
//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"
//#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include "stb_image_write.h"
//#define QOI_IMPLEMENTATION
//#include "qoi.h"
//
//// Utility functions
//int64_t get_time_ns() {
//    return std::chrono::duration_cast<std::chrono::nanoseconds>(
//        std::chrono::high_resolution_clock::now().time_since_epoch()
//    ).count();
//}
//
//std::string format_duration(double milliseconds) {
//    std::ostringstream oss;
//    oss << std::fixed << std::setprecision(3) << milliseconds;
//    return oss.str();
//}
//
//std::string format_size(int size) {
//    if (size < 1024) return std::to_string(size) + " B";
//    if (size < 1024 * 1024) return std::to_string(size / 1024) + " KB";
//    return std::to_string(size / (1024 * 1024)) + " MB";
//}
//
//double calculate_speedup(double sequential_time, double parallel_time) {
//    return sequential_time / parallel_time;
//}
//
//// Performance metrics structure
//struct PerformanceMetrics {
//    int64_t load_time;
//    int64_t process_time;
//    int64_t save_time;
//    int total_size;
//    float compression_ratio;
//    int width;
//    int height;
//    int channels;
//};
//
//// Sequential encode function
//PerformanceMetrics encode_file(const std::string& input_path, const std::string& output_path) {
//    PerformanceMetrics metrics = { 0 };
//    int64_t start_time;
//
//    // Load image
//    start_time = get_time_ns();
//    unsigned char* data = stbi_load(input_path.c_str(), &metrics.width, &metrics.height, &metrics.channels, 0);
//    metrics.load_time = get_time_ns() - start_time;
//
//    if (!data) {
//        printf("Failed to load image: %s\n", input_path.c_str());
//        return metrics;
//    }
//
//    qoi_desc desc = {
//        static_cast<unsigned int>(metrics.width),
//        static_cast<unsigned int>(metrics.height),
//        metrics.channels,
//        QOI_SRGB
//    };
//
//    // Encode
//    start_time = get_time_ns();
//    int encoded_size;
//    void* encoded_data = qoi_encode(data, &desc, &encoded_size);
//    metrics.process_time = get_time_ns() - start_time;
//
//    if (!encoded_data) {
//        printf("Failed to encode image: %s\n", input_path.c_str());
//        stbi_image_free(data);
//        return metrics;
//    }
//
//    // Save
//    start_time = get_time_ns();
//    FILE* f = fopen(output_path.c_str(), "wb");
//    if (f) {
//        fwrite(encoded_data, 1, encoded_size, f);
//        fclose(f);
//    }
//    metrics.save_time = get_time_ns() - start_time;
//
//    // Calculate metrics
//    int original_size = metrics.width * metrics.height * metrics.channels;
//    metrics.total_size = encoded_size;
//    metrics.compression_ratio = (1.0f - (float)encoded_size / original_size) * 100;
//
//    // Cleanup
//    free(encoded_data);
//    stbi_image_free(data);
//    return metrics;
//}
//
//// Sequential decode function
//PerformanceMetrics decode_file(const std::string& input_path, const std::string& output_path) {
//    PerformanceMetrics metrics = { 0 };
//    int64_t start_time;
//
//    // Load file
//    start_time = get_time_ns();
//    FILE* f = fopen(input_path.c_str(), "rb");
//    if (!f) {
//        printf("Failed to open QOI file: %s\n", input_path.c_str());
//        return metrics;
//    }
//
//    fseek(f, 0, SEEK_END);
//    int file_size = ftell(f);
//    fseek(f, 0, SEEK_SET);
//
//    void* raw_data = malloc(file_size);
//    fread(raw_data, 1, file_size, f);
//    fclose(f);
//    metrics.load_time = get_time_ns() - start_time;
//
//    // Decode
//    start_time = get_time_ns();
//    qoi_desc desc;
//    void* decoded_data = qoi_decode(raw_data, file_size, &desc, 0);
//    free(raw_data);
//    metrics.process_time = get_time_ns() - start_time;
//
//    if (!decoded_data) {
//        printf("Failed to decode QOI file: %s\n", input_path.c_str());
//        return metrics;
//    }
//
//    metrics.width = desc.width;
//    metrics.height = desc.height;
//    metrics.channels = desc.channels;
//
//    // Save
//    start_time = get_time_ns();
//    stbi_write_png(output_path.c_str(), desc.width, desc.height,
//        desc.channels, decoded_data, desc.width * desc.channels);
//    metrics.save_time = get_time_ns() - start_time;
//
//    metrics.total_size = desc.width * desc.height * desc.channels;
//    free(decoded_data);
//    return metrics;
//}
//
//PerformanceMetrics encode_file_parallel(const std::string& input_path, const std::string& output_path) {
//    PerformanceMetrics metrics = { 0 };
//    int64_t start_time;
//
//    // Load image
//    start_time = get_time_ns();
//    unsigned char* data = stbi_load(input_path.c_str(), &metrics.width, &metrics.height, &metrics.channels, 0);
//    metrics.load_time = get_time_ns() - start_time;
//
//    if (!data) {
//        printf("Failed to load image: %s\n", input_path.c_str());
//        return metrics;
//    }
//
//    qoi_desc desc = {
//        static_cast<unsigned int>(metrics.width),
//        static_cast<unsigned int>(metrics.height),
//        metrics.channels,
//        QOI_SRGB
//    };
//
//    // Encode (Parallel)
//    start_time = get_time_ns();
//    int encoded_size;
//    void* encoded_data = qoi_encode_parallel_optimized(data, &desc, &encoded_size);
//    metrics.process_time = get_time_ns() - start_time;
//
//    if (!encoded_data) {
//        printf("Failed to encode image: %s\n", input_path.c_str());
//        stbi_image_free(data);
//        return metrics;
//    }
//
//    // Save
//    start_time = get_time_ns();
//    FILE* f = fopen(output_path.c_str(), "wb");
//    if (f) {
//        fwrite(encoded_data, 1, encoded_size, f);
//        fclose(f);
//    }
//    metrics.save_time = get_time_ns() - start_time;
//
//    // Calculate metrics
//    int original_size = metrics.width * metrics.height * metrics.channels;
//    metrics.total_size = encoded_size;
//    metrics.compression_ratio = (1.0f - (float)encoded_size / original_size) * 100;
//
//    // Cleanup
//    free(encoded_data);
//    stbi_image_free(data);
//    return metrics;
//}
//
//PerformanceMetrics decode_file_parallel(const std::string& input_path, const std::string& output_path) {
//    PerformanceMetrics metrics = { 0 };
//    int64_t start_time;
//
//    // Load file
//    start_time = get_time_ns();
//    FILE* f = fopen(input_path.c_str(), "rb");
//    if (!f) {
//        printf("Failed to open QOI file: %s\n", input_path.c_str());
//        return metrics;
//    }
//
//    fseek(f, 0, SEEK_END);
//    int file_size = ftell(f);
//    fseek(f, 0, SEEK_SET);
//
//    void* raw_data = malloc(file_size);
//    fread(raw_data, 1, file_size, f);
//    fclose(f);
//    metrics.load_time = get_time_ns() - start_time;
//
//    // Decode (Parallel)
//    start_time = get_time_ns();
//    qoi_desc desc;
//    void* decoded_data = qoi_decode_parallel_optimized(raw_data, file_size, &desc, 0);
//    free(raw_data);
//    metrics.process_time = get_time_ns() - start_time;
//
//    if (!decoded_data) {
//        printf("Failed to decode QOI file: %s\n", input_path.c_str());
//        return metrics;
//    }
//
//    metrics.width = desc.width;
//    metrics.height = desc.height;
//    metrics.channels = desc.channels;
//
//    // Save
//    start_time = get_time_ns();
//    stbi_write_png(output_path.c_str(), desc.width, desc.height,
//        desc.channels, decoded_data, desc.width * desc.channels);
//    metrics.save_time = get_time_ns() - start_time;
//
//    metrics.total_size = desc.width * desc.height * desc.channels;
//    free(decoded_data);
//    return metrics;
//}
//
//
//// Print comparison results
//void print_comparison_results(const std::string& filename,
//    const PerformanceMetrics& seq_metrics,
//    const PerformanceMetrics& par_metrics,
//    int num_threads) {
//    printf("+==============================================================================+\n");
//    printf("| PERFORMANCE COMPARISON: %-56s |\n", filename.c_str());
//    printf("+==============================================================================+\n");
//    printf("| Image Info:                                                                   |\n");
//    printf("| Dimensions: %dx%d, Channels: %d                                               |\n",
//        seq_metrics.width, seq_metrics.height, seq_metrics.channels);
//    printf("+----------------+----------------+--------------------+------------------+\n");
//    printf("| Metric          | Sequential      | Parallel (%d threads) | Speedup          |\n", num_threads);
//    printf("+----------------+----------------+--------------------+------------------+\n");
//
//    double seq_load = seq_metrics.load_time / 1e6;
//    double par_load = par_metrics.load_time / 1e6;
//    printf("| Load Time      | %8.3f ms    | %8.3f ms        | %7.2fx         |\n",
//        seq_load, par_load, calculate_speedup(seq_load, par_load));
//
//    double seq_process = seq_metrics.process_time / 1e6;
//    double par_process = par_metrics.process_time / 1e6;
//    printf("| Process Time   | %8.3f ms    | %8.3f ms        | %7.2fx         |\n",
//        seq_process, par_process, calculate_speedup(seq_process, par_process));
//
//    double seq_save = seq_metrics.save_time / 1e6;
//    double par_save = par_metrics.save_time / 1e6;
//    printf("| Save Time      | %8.3f ms    | %8.3f ms        | %7.2fx         |\n",
//        seq_save, par_save, calculate_speedup(seq_save, par_save));
//
//    double seq_total = (seq_metrics.load_time + seq_metrics.process_time + seq_metrics.save_time) / 1e6;
//    double par_total = (par_metrics.load_time + par_metrics.process_time + par_metrics.save_time) / 1e6;
//    printf("| Total Time     | %8.3f ms    | %8.3f ms        | %7.2fx         |\n",
//        seq_total, par_total, calculate_speedup(seq_total, par_total));
//
//    printf("+----------------+----------------+--------------------+------------------+\n");
//    printf("| File Size      | %-10s    | %-10s        | %-16s |\n",
//        format_size(seq_metrics.total_size).c_str(),
//        format_size(par_metrics.total_size).c_str(), "-");
//    printf("| Compression    | %7.2f%%      | %7.2f%%          | %-16s |\n",
//        seq_metrics.compression_ratio, par_metrics.compression_ratio, "-");
//    printf("+==============================================================================+\n\n");
//}
//
//// Main function
//int main(int argc, char* argv[]) {
//    setvbuf(stdout, NULL, _IONBF, 0);
//
//    if (argc != 5) {
//        printf("Usage: %s [encode|decode] <input_dir> <output_dir> <num_threads>\n", argv[0]);
//        return 1;
//    }
//
//    const char* mode = argv[1];
//    const char* input_dir = argv[2];
//    const char* output_dir = argv[3];
//    const int num_threads = atoi(argv[4]);
//
//    printf("Mode: %s\n", mode);
//    printf("Input Directory: %s\n", input_dir);
//    printf("Output Directory: %s\n", output_dir);
//    printf("Threads: %d\n\n", num_threads);
//
//    // Ensure the output directory exists
//    CreateDirectoryA(output_dir, NULL);
//
//    WIN32_FIND_DATAA findData;
//    char search_path[MAX_PATH];
//
//    if (strcmp(mode, "encode") == 0) {
//        sprintf_s(search_path, "%s\\*.*", input_dir);
//        HANDLE hFind = FindFirstFileA(search_path, &findData);
//        if (hFind == INVALID_HANDLE_VALUE) {
//            printf("No files found in input directory\n");
//            return 1;
//        }
//
//        do {
//            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
//                char* ext = strrchr(findData.cFileName, '.');
//                if (ext && (_stricmp(ext, ".png") == 0 || _stricmp(ext, ".jpg") == 0 || _stricmp(ext, ".jpeg") == 0)) {
//                    char input_path[MAX_PATH];
//                    char seq_output_path[MAX_PATH];
//                    char par_output_path[MAX_PATH];
//
//                    sprintf_s(input_path, "%s\\%s", input_dir, findData.cFileName);
//                    sprintf_s(seq_output_path, "%s\\seq_%.*s.qoi", output_dir, (int)(ext - findData.cFileName), findData.cFileName);
//                    sprintf_s(par_output_path, "%s\\par_%.*s.qoi", output_dir, (int)(ext - findData.cFileName), findData.cFileName);
//
//                    printf("Processing: %s\n", findData.cFileName);
//
//                    // Sequential encoding
//                    PerformanceMetrics seq_metrics = encode_file(input_path, seq_output_path);
//
//                    // Parallel encoding
//                    PerformanceMetrics par_metrics = encode_file_parallel(input_path, par_output_path);
//
//                    // Print comparison results
//                    print_comparison_results(findData.cFileName, seq_metrics, par_metrics, num_threads);
//                }
//            }
//        } while (FindNextFileA(hFind, &findData));
//        FindClose(hFind);
//    }
//    else if (strcmp(mode, "decode") == 0) {
//        sprintf_s(search_path, "%s\\*.qoi", input_dir);
//        HANDLE hFind = FindFirstFileA(search_path, &findData);
//        if (hFind == INVALID_HANDLE_VALUE) {
//            printf("No .qoi files found in input directory\n");
//            return 1;
//        }
//
//        do {
//            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
//                char input_path[MAX_PATH];
//                char seq_output_path[MAX_PATH];
//                char par_output_path[MAX_PATH];
//
//                sprintf_s(input_path, "%s\\%s", input_dir, findData.cFileName);
//                sprintf_s(seq_output_path, "%s\\seq_%.*s.png", output_dir, (int)(strlen(findData.cFileName) - 4), findData.cFileName);
//                sprintf_s(par_output_path, "%s\\par_%.*s.png", output_dir, (int)(strlen(findData.cFileName) - 4), findData.cFileName);
//
//                printf("Processing: %s\n", findData.cFileName);
//
//                // Sequential decoding
//                PerformanceMetrics seq_metrics = decode_file(input_path, seq_output_path);
//
//                // Parallel decoding
//                PerformanceMetrics par_metrics = decode_file_parallel(input_path, par_output_path);
//
//                // Print comparison results
//                print_comparison_results(findData.cFileName, seq_metrics, par_metrics, num_threads);
//            }
//        } while (FindNextFileA(hFind, &findData));
//        FindClose(hFind);
//    }
//    else {
//        printf("Invalid mode. Use 'encode' or 'decode'.\n");
//        return 1;
//    }
//
//    return 0;
//}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <Windows.h>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <omp.h>
#include <vector>
#include <map>

// ... (keep all the includes and utility functions)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define QOI_IMPLEMENTATION
#include "qoi.h"

// Utility functions
int64_t get_time_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

std::string format_duration(double milliseconds) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << milliseconds;
    return oss.str();
}

std::string format_size(int size) {
    if (size < 1024) return std::to_string(size) + " B";
    if (size < 1024 * 1024) return std::to_string(size / 1024) + " KB";
    return std::to_string(size / (1024 * 1024)) + " MB";
}

double calculate_speedup(double sequential_time, double parallel_time) {
    return sequential_time / parallel_time;
}

struct ProcessingResult {
    std::string filename;
    double processing_time;  // in milliseconds
    size_t file_size;
    std::string output_path;
    int width;
    int height;
    int channels;
};

// Helper function to get file size
size_t get_file_size(const char* filepath) {
    struct stat file_status;
    if (stat(filepath, &file_status) < 0) {
        return 0;
    }
    return file_status.st_size;
}


ProcessingResult encode_file(const std::string& input_path, const std::string& output_path,
    bool is_parallel, int num_threads,
    std::vector<ProcessingResult>& seq_results,
    std::vector<std::vector<ProcessingResult>>& par_results_multi,
    size_t thread_index,
    bool print_table) {

    ProcessingResult result;
    result.filename = input_path.substr(input_path.find_last_of("/\\") + 1);
    result.output_path = output_path;
    int width, height, channels;
    unsigned char* data = stbi_load(input_path.c_str(), &width, &height, &channels, 0);
    if (!data) {
        printf("Failed to load image: %s\n", input_path.c_str());
        return result;
    }
    result.width = width;
    result.height = height;
    result.channels = channels;
    qoi_desc desc = {
        static_cast<unsigned int>(width),
        static_cast<unsigned int>(height),
        channels,
        QOI_SRGB
    };

    int64_t start_time = get_time_ns();
    int encoded_size;
    void* encoded_data = is_parallel ?
        qoi_encode_parallel_block_simple(data, &desc, &encoded_size, num_threads) :
        qoi_encode_modify(data, &desc, &encoded_size);
    result.processing_time = (get_time_ns() - start_time) / 1e6;

    if (encoded_data) {
        FILE* f = fopen(output_path.c_str(), "wb");
        if (f) {
            fwrite(encoded_data, 1, encoded_size, f);
            fclose(f);
        }
        free(encoded_data);
    }
    stbi_image_free(data);

    // Store results in appropriate vectors
    if (is_parallel) {
        par_results_multi[thread_index].push_back(result);
    }
    else {
        seq_results.push_back(result);
    }

    return result;
}

ProcessingResult decode_file(const std::string& input_path, const std::string& output_path,
    bool is_parallel, int num_threads,
    std::vector<ProcessingResult>& seq_results,
    std::vector<std::vector<ProcessingResult>>& par_results_multi,
    size_t thread_index,
    bool print_table) {  // Add flag to control table printing

    ProcessingResult result;
    result.filename = input_path.substr(input_path.find_last_of("/\\") + 1);
    result.output_path = output_path;

    FILE* f = fopen(input_path.c_str(), "rb");
    if (!f) {
        printf("Failed to open QOI file: %s\n", input_path.c_str());
        return result;
    }

    fseek(f, 0, SEEK_END);
    int file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void* raw_data = malloc(file_size);
    fread(raw_data, 1, file_size, f);
    fclose(f);

    int64_t start_time = get_time_ns();
    qoi_desc desc;
    void* decoded_data = is_parallel ?
        qoi_decode_parallel_block_simple(raw_data, file_size, &desc, 0, num_threads) :
        qoi_decode_modify(raw_data, file_size, &desc, 0);
    result.processing_time = (get_time_ns() - start_time) / 1e6;

    if (decoded_data) {
        stbi_write_png(output_path.c_str(), desc.width, desc.height,
            desc.channels, decoded_data, desc.width * desc.channels);
        free(decoded_data);
    }
    free(raw_data);
    result.width = desc.width;
    result.height = desc.height;
    result.channels = desc.channels;

    // Store results in appropriate vectors
    if (is_parallel) {
        par_results_multi[thread_index].push_back(result);
    }
    else {
        seq_results.push_back(result);
    }

    return result;
}

// Add this function after the ProcessingResult struct definition
void save_performance_data(const std::string& output_file,
    const std::vector<ProcessingResult>& seq_encode,
    const std::vector<std::vector<ProcessingResult>>& par_encode_multi,
    const std::vector<ProcessingResult>& seq_decode,
    const std::vector<std::vector<ProcessingResult>>& par_decode_multi,
    const std::vector<int>& thread_counts) {
    FILE* f = fopen(output_file.c_str(), "a");
    if (!f) {
        printf("Failed to create/open performance data file\n");
        return;
    }
    // Write CSV header
    fprintf(f, "Operation,ImageSize,ThreadCount,ProcessingTime,ImageWidth,ImageHeight,TotalPixels,FileSize\n");

    auto write_result = [f](const char* operation, const ProcessingResult& result, int thread_count) {
        int total_pixels = result.width * result.height;

        // Use the output_path from the ProcessingResult
        size_t actual_file_size = get_file_size(result.output_path.c_str());

        fprintf(f, "%s,%dx%d,%d,%.3f,%d,%d,%d,%zu\n",
            operation,
            result.width, result.height,
            thread_count,
            result.processing_time,
            result.width, result.height,
            total_pixels,
            actual_file_size);
        };

    // Write encoding results
    if (!seq_encode.empty()) {
        for (const auto& result : seq_encode) {
            write_result("Encode", result, 1);
        }
        for (size_t t = 0; t < thread_counts.size() && t < par_encode_multi.size(); t++) {
            for (const auto& result : par_encode_multi[t]) {
                write_result("Encode", result, thread_counts[t]);
            }
        }
    }

    // Write decoding results
    if (!seq_decode.empty()) {
        for (const auto& result : seq_decode) {
            write_result("Decode", result, 1);
        }
        for (size_t t = 0; t < thread_counts.size() && t < par_decode_multi.size(); t++) {
            for (const auto& result : par_decode_multi[t]) {
                write_result("Decode", result, thread_counts[t]);
            }
        }
    }

    fclose(f);
    printf("\nPerformance data saved to: %s\n", output_file.c_str());
}

int main(int argc, char* argv[]){
    if (argc < 5) {
        printf("Usage: %s <mode> <input_dir> <output_dir> <thread_counts...>\n", argv[0]);
        printf("Example: %s encode ./input ./output 2 4 8\n", argv[0]);
        return 1;
    }

    const char* mode = argv[1];
    const char* input_dir = argv[2];
    const char* output_dir = argv[3];

    // Parse thread counts directly from argv
    std::vector<int> thread_counts;
    for (int i = 4; i < argc; i++) {
        thread_counts.push_back(atoi(argv[i]));
    }

    CreateDirectoryA(output_dir, NULL);

    std::vector<std::vector<ProcessingResult>> parallel_encode_results_multi;
    std::vector<std::vector<ProcessingResult>> parallel_decode_results_multi;
    std::vector<ProcessingResult> sequential_encode_results;
    std::vector<ProcessingResult> sequential_decode_results;

    for (size_t i = 0; i < thread_counts.size(); i++) {
        parallel_encode_results_multi.push_back(std::vector<ProcessingResult>());
        parallel_decode_results_multi.push_back(std::vector<ProcessingResult>());
    }

    if (strcmp(mode, "encode") == 0) {
        WIN32_FIND_DATAA findData;
        char search_path[MAX_PATH];
        sprintf_s(search_path, "%s\\*.*", input_dir);
        std::vector<std::string> input_files;  // Store files to process

        HANDLE hFind = FindFirstFileA(search_path, &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    char* ext = strrchr(findData.cFileName, '.');
                    if (ext && (_stricmp(ext, ".png") == 0 || _stricmp(ext, ".jpg") == 0 || _stricmp(ext, ".jpeg") == 0)) {
                        input_files.push_back(findData.cFileName);
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }

        // Process each file
        for (const auto& filename : input_files) {
            char input_path[MAX_PATH];
            char seq_output_path[MAX_PATH];

            // Get the extension position safely
            size_t ext_pos = filename.find_last_of('.');
            if (ext_pos == std::string::npos) continue;  // Skip if no extension found

            sprintf_s(input_path, "%s\\%s", input_dir, filename.c_str());
            sprintf_s(seq_output_path, "%s\\seq_%.*s.qoi", output_dir,
                static_cast<int>(ext_pos), filename.c_str());

            printf("Encoding: %s\n", filename.c_str());

            // Get input file size
            size_t input_file_size = get_file_size(input_path);

            // Sequential encoding
            ProcessingResult seq_result = encode_file(input_path, seq_output_path, false, 0,
                sequential_encode_results,
                parallel_encode_results_multi,
                0,  // thread_index doesn't matter for sequential
                false  // Don't print table for sequential
            );
            seq_result.file_size = input_file_size;

            // Parallel encoding with different thread counts
            for (size_t i = 0; i < thread_counts.size(); i++) {
                char par_output_path[MAX_PATH];
                sprintf_s(par_output_path, "%s\\par_%d_%.*s.qoi", output_dir, thread_counts[i],
                    static_cast<int>(ext_pos), filename.c_str());

                ProcessingResult par_result = encode_file(input_path, par_output_path, true, thread_counts[i],
                    sequential_encode_results,
                    parallel_encode_results_multi,
                    i,  // Pass the current thread index
                    false  // Don't print table yet
                );
                par_result.file_size = input_file_size;
            }
        }

        // After all files are processed, print comparison tables for each thread count
        for (size_t t = 0; t < thread_counts.size(); t++) {
            if (parallel_encode_results_multi[t].size() == sequential_encode_results.size()) {
                double total_seq_time = 0, total_par_time = 0;

                printf("\n+=========================================================================+\n");
                printf("|                    ENCODING PERFORMANCE COMPARISON                        |\n");
                printf("|                        (%d threads for parallel)                          |\n", thread_counts[t]);
                printf("+----------------------+-------------+-------------+-------------+----------+\n");
                printf("| Image                | Dimensions  | Sequential  | Parallel    | Speedup  |\n");
                printf("+----------------------+-------------+-------------+-------------+----------+\n");

                for (size_t i = 0; i < sequential_encode_results.size(); i++) {
                    auto& seq = sequential_encode_results[i];
                    auto& par = parallel_encode_results_multi[t][i];
                    double speedup = seq.processing_time / par.processing_time;

                    total_seq_time += seq.processing_time;
                    total_par_time += par.processing_time;

                    printf("| %-20s | %4dx%4d   | %7.2f ms  | %7.2f ms  | %6.2fx  |\n",
                        seq.filename.c_str(), seq.width, seq.height,
                        seq.processing_time, par.processing_time, speedup);
                }

                double avg_seq = total_seq_time / sequential_encode_results.size();
                double avg_par = total_par_time / parallel_encode_results_multi[t].size();
                double overall_speedup = total_seq_time / total_par_time;

                printf("+----------------------+-------------+-------------+-------------+----------+\n");
                printf("| TOTAL                | -           | %7.2f ms  | %7.2f ms  | %6.2fx  |\n",
                    total_seq_time, total_par_time, overall_speedup);
                printf("| AVERAGE              | -           | %7.2f ms  | %7.2f ms  | -        |\n",
                    avg_seq, avg_par);
                printf("+----------------------+-------------+-------------+-------------+----------+\n\n");
            }
        }
    }
    else if (strcmp(mode, "decode") == 0) {
        char search_path[MAX_PATH];
        WIN32_FIND_DATAA findData;
        std::vector<std::string> input_files;  // Store files to process

        // First, collect all files to process
        sprintf_s(search_path, "%s\\seq_*.qoi", input_dir);
        HANDLE hFind = FindFirstFileA(search_path, &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                input_files.push_back(findData.cFileName);
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }

        // Process each file
        for (const auto& filename : input_files) {
            char input_path[MAX_PATH];
            char output_path[MAX_PATH];

            sprintf_s(input_path, "%s\\%s", input_dir, filename.c_str());
            sprintf_s(output_path, "%s\\decoded_%s.png", output_dir, filename.c_str());

            printf("Decoding sequential: %s\n", filename.c_str());

            // Get QOI file size
            size_t qoi_file_size = get_file_size(input_path);

            // Sequential decoding
            ProcessingResult seq_result = decode_file(input_path, output_path, false, 0,
                sequential_decode_results,
                parallel_decode_results_multi,
                0,  // thread_index doesn't matter for sequential
                false  // Don't print table for sequential
            );
            seq_result.file_size = qoi_file_size;

            // Process parallel decoding for each thread count
            for (size_t t = 0; t < thread_counts.size(); t++) {
                sprintf_s(output_path, "%s\\decoded_%d_%s.png", output_dir, thread_counts[t], filename.c_str());
                ProcessingResult par_result = decode_file(input_path, output_path, true, thread_counts[t],
                    sequential_decode_results,
                    parallel_decode_results_multi,
                    t,  // Pass the current thread index
                    false  // Don't print table yet
                );
                par_result.file_size = qoi_file_size;
            }
        }
        // After all files are processed, print comparison tables for each thread count
        for (size_t t = 0; t < thread_counts.size(); t++) {
            // Print comparison table for current thread count
            if (parallel_decode_results_multi[t].size() == sequential_decode_results.size()) {
                double total_seq_time = 0, total_par_time = 0;

                printf("\n+=========================================================================+\n");
                printf("|                    DECODING PERFORMANCE COMPARISON                        |\n");
                printf("|                        (%d threads for parallel)                          |\n", thread_counts[t]);
                printf("+----------------------+-------------+-------------+-------------+----------+\n");
                printf("| Image                | Dimensions  | Sequential  | Parallel    | Speedup  |\n");
                printf("+----------------------+-------------+-------------+-------------+----------+\n");

                for (size_t i = 0; i < sequential_decode_results.size(); i++) {
                    auto& seq = sequential_decode_results[i];
                    auto& par = parallel_decode_results_multi[t][i];
                    double speedup = seq.processing_time / par.processing_time;

                    total_seq_time += seq.processing_time;
                    total_par_time += par.processing_time;

                    printf("| %-20s | %4dx%4d   | %7.2f ms  | %7.2f ms  | %6.2fx  |\n",
                        seq.filename.c_str(), seq.width, seq.height,
                        seq.processing_time, par.processing_time, speedup);
                }

                double avg_seq = total_seq_time / sequential_decode_results.size();
                double avg_par = total_par_time / parallel_decode_results_multi[t].size();
                double overall_speedup = total_seq_time / total_par_time;

                printf("+----------------------+-------------+-------------+-------------+----------+\n");
                printf("| TOTAL                | -           | %7.2f ms  | %7.2f ms  | %6.2fx  |\n",
                    total_seq_time, total_par_time, overall_speedup);
                printf("| AVERAGE              | -           | %7.2f ms  | %7.2f ms  | -        |\n",
                    avg_seq, avg_par);
                printf("+----------------------+-------------+-------------+-------------+----------+\n\n");
            }
        }
    }

        // Save performance data to CSV
        std::string csv_path = "performance_data_multi.csv";
        save_performance_data(csv_path,
            sequential_encode_results,
            parallel_encode_results_multi,
            sequential_decode_results,
            parallel_decode_results_multi,
            thread_counts);
 
    return 0;
}
