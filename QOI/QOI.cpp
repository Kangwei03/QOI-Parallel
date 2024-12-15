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

// Performance metrics structure
struct PerformanceMetrics {
    int64_t load_time;
    int64_t process_time;
    int64_t save_time;
    int total_size;
    float compression_ratio;
    int width;
    int height;
    int channels;
};

// Sequential encode function
PerformanceMetrics encode_file(const std::string& input_path, const std::string& output_path) {
    PerformanceMetrics metrics = { 0 };
    int64_t start_time;

    // Load image
    start_time = get_time_ns();
    unsigned char* data = stbi_load(input_path.c_str(), &metrics.width, &metrics.height, &metrics.channels, 0);
    metrics.load_time = get_time_ns() - start_time;

    if (!data) {
        printf("Failed to load image: %s\n", input_path.c_str());
        return metrics;
    }

    qoi_desc desc = {
        static_cast<unsigned int>(metrics.width),
        static_cast<unsigned int>(metrics.height),
        metrics.channels,
        QOI_SRGB
    };

    // Encode
    start_time = get_time_ns();
    int encoded_size;
    void* encoded_data = qoi_encode(data, &desc, &encoded_size);
    metrics.process_time = get_time_ns() - start_time;

    if (!encoded_data) {
        printf("Failed to encode image: %s\n", input_path.c_str());
        stbi_image_free(data);
        return metrics;
    }

    // Save
    start_time = get_time_ns();
    FILE* f = fopen(output_path.c_str(), "wb");
    if (f) {
        fwrite(encoded_data, 1, encoded_size, f);
        fclose(f);
    }
    metrics.save_time = get_time_ns() - start_time;

    // Calculate metrics
    int original_size = metrics.width * metrics.height * metrics.channels;
    metrics.total_size = encoded_size;
    metrics.compression_ratio = (1.0f - (float)encoded_size / original_size) * 100;

    // Cleanup
    free(encoded_data);
    stbi_image_free(data);
    return metrics;
}

// Sequential decode function
PerformanceMetrics decode_file(const std::string& input_path, const std::string& output_path) {
    PerformanceMetrics metrics = { 0 };
    int64_t start_time;

    // Load file
    start_time = get_time_ns();
    FILE* f = fopen(input_path.c_str(), "rb");
    if (!f) {
        printf("Failed to open QOI file: %s\n", input_path.c_str());
        return metrics;
    }

    fseek(f, 0, SEEK_END);
    int file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    void* raw_data = malloc(file_size);
    fread(raw_data, 1, file_size, f);
    fclose(f);
    metrics.load_time = get_time_ns() - start_time;

    // Decode
    start_time = get_time_ns();
    qoi_desc desc;
    void* decoded_data = qoi_decode(raw_data, file_size, &desc, 0);
    free(raw_data);
    metrics.process_time = get_time_ns() - start_time;

    if (!decoded_data) {
        printf("Failed to decode QOI file: %s\n", input_path.c_str());
        return metrics;
    }

    metrics.width = desc.width;
    metrics.height = desc.height;
    metrics.channels = desc.channels;

    // Save
    start_time = get_time_ns();
    stbi_write_png(output_path.c_str(), desc.width, desc.height,
        desc.channels, decoded_data, desc.width * desc.channels);
    metrics.save_time = get_time_ns() - start_time;

    metrics.total_size = desc.width * desc.height * desc.channels;
    free(decoded_data);
    return metrics;
}

PerformanceMetrics encode_file_parallel(const std::string& input_path, const std::string& output_path) {
    PerformanceMetrics metrics = { 0 };
    int64_t start_time;

    // Load image
    start_time = get_time_ns();
    unsigned char* data = stbi_load(input_path.c_str(), &metrics.width, &metrics.height, &metrics.channels, 0);
    metrics.load_time = get_time_ns() - start_time;

    if (!data) {
        printf("Failed to load image: %s\n", input_path.c_str());
        return metrics;
    }

    qoi_desc desc = {
        static_cast<unsigned int>(metrics.width),
        static_cast<unsigned int>(metrics.height),
        metrics.channels,
        QOI_SRGB
    };

    // Encode (Parallel)
    start_time = get_time_ns();
    int encoded_size;
    void* encoded_data = qoi_encode_parallel(data, &desc, &encoded_size);
    metrics.process_time = get_time_ns() - start_time;

    if (!encoded_data) {
        printf("Failed to encode image: %s\n", input_path.c_str());
        stbi_image_free(data);
        return metrics;
    }

    // Save
    start_time = get_time_ns();
    FILE* f = fopen(output_path.c_str(), "wb");
    if (f) {
        fwrite(encoded_data, 1, encoded_size, f);
        fclose(f);
    }
    metrics.save_time = get_time_ns() - start_time;

    // Calculate metrics
    int original_size = metrics.width * metrics.height * metrics.channels;
    metrics.total_size = encoded_size;
    metrics.compression_ratio = (1.0f - (float)encoded_size / original_size) * 100;

    // Cleanup
    free(encoded_data);
    stbi_image_free(data);
    return metrics;
}

PerformanceMetrics decode_file_parallel(const std::string& input_path, const std::string& output_path) {
    PerformanceMetrics metrics = { 0 };
    int64_t start_time;

    // Load file
    start_time = get_time_ns();
    FILE* f = fopen(input_path.c_str(), "rb");
    if (!f) {
        printf("Failed to open QOI file: %s\n", input_path.c_str());
        return metrics;
    }

    fseek(f, 0, SEEK_END);
    int file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    void* raw_data = malloc(file_size);
    fread(raw_data, 1, file_size, f);
    fclose(f);
    metrics.load_time = get_time_ns() - start_time;

    // Decode (Parallel)
    start_time = get_time_ns();
    qoi_desc desc;
    void* decoded_data = qoi_decode_parallel(raw_data, file_size, &desc, 0);
    free(raw_data);
    metrics.process_time = get_time_ns() - start_time;

    if (!decoded_data) {
        printf("Failed to decode QOI file: %s\n", input_path.c_str());
        return metrics;
    }

    metrics.width = desc.width;
    metrics.height = desc.height;
    metrics.channels = desc.channels;

    // Save
    start_time = get_time_ns();
    stbi_write_png(output_path.c_str(), desc.width, desc.height,
        desc.channels, decoded_data, desc.width * desc.channels);
    metrics.save_time = get_time_ns() - start_time;

    metrics.total_size = desc.width * desc.height * desc.channels;
    free(decoded_data);
    return metrics;
}


// Print comparison results
void print_comparison_results(const std::string& filename,
    const PerformanceMetrics& seq_metrics,
    const PerformanceMetrics& par_metrics,
    int num_threads) {
    printf("+==============================================================================+\n");
    printf("| PERFORMANCE COMPARISON: %-56s |\n", filename.c_str());
    printf("+==============================================================================+\n");
    printf("| Image Info:                                                                   |\n");
    printf("| Dimensions: %dx%d, Channels: %d                                               |\n",
        seq_metrics.width, seq_metrics.height, seq_metrics.channels);
    printf("+----------------+----------------+--------------------+------------------+\n");
    printf("| Metric          | Sequential      | Parallel (%d threads) | Speedup          |\n", num_threads);
    printf("+----------------+----------------+--------------------+------------------+\n");

    double seq_load = seq_metrics.load_time / 1e6;
    double par_load = par_metrics.load_time / 1e6;
    printf("| Load Time      | %8.3f ms    | %8.3f ms        | %7.2fx         |\n",
        seq_load, par_load, calculate_speedup(seq_load, par_load));

    double seq_process = seq_metrics.process_time / 1e6;
    double par_process = par_metrics.process_time / 1e6;
    printf("| Process Time   | %8.3f ms    | %8.3f ms        | %7.2fx         |\n",
        seq_process, par_process, calculate_speedup(seq_process, par_process));

    double seq_save = seq_metrics.save_time / 1e6;
    double par_save = par_metrics.save_time / 1e6;
    printf("| Save Time      | %8.3f ms    | %8.3f ms        | %7.2fx         |\n",
        seq_save, par_save, calculate_speedup(seq_save, par_save));

    double seq_total = (seq_metrics.load_time + seq_metrics.process_time + seq_metrics.save_time) / 1e6;
    double par_total = (par_metrics.load_time + par_metrics.process_time + par_metrics.save_time) / 1e6;
    printf("| Total Time     | %8.3f ms    | %8.3f ms        | %7.2fx         |\n",
        seq_total, par_total, calculate_speedup(seq_total, par_total));

    printf("+----------------+----------------+--------------------+------------------+\n");
    printf("| File Size      | %-10s    | %-10s        | %-16s |\n",
        format_size(seq_metrics.total_size).c_str(),
        format_size(par_metrics.total_size).c_str(), "-");
    printf("| Compression    | %7.2f%%      | %7.2f%%          | %-16s |\n",
        seq_metrics.compression_ratio, par_metrics.compression_ratio, "-");
    printf("+==============================================================================+\n\n");
}

// Main function
int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc != 5) {
        printf("Usage: %s [encode|decode] <input_dir> <output_dir> <num_threads>\n", argv[0]);
        return 1;
    }

    const char* mode = argv[1];
    const char* input_dir = argv[2];
    const char* output_dir = argv[3];
    const int num_threads = atoi(argv[4]);

    printf("Mode: %s\n", mode);
    printf("Input Directory: %s\n", input_dir);
    printf("Output Directory: %s\n", output_dir);
    printf("Threads: %d\n\n", num_threads);

    // Ensure the output directory exists
    CreateDirectoryA(output_dir, NULL);

    WIN32_FIND_DATAA findData;
    char search_path[MAX_PATH];

    if (strcmp(mode, "encode") == 0) {
        sprintf_s(search_path, "%s\\*.*", input_dir);
        HANDLE hFind = FindFirstFileA(search_path, &findData);
        if (hFind == INVALID_HANDLE_VALUE) {
            printf("No files found in input directory\n");
            return 1;
        }

        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char* ext = strrchr(findData.cFileName, '.');
                if (ext && (_stricmp(ext, ".png") == 0 || _stricmp(ext, ".jpg") == 0 || _stricmp(ext, ".jpeg") == 0)) {
                    char input_path[MAX_PATH];
                    char seq_output_path[MAX_PATH];
                    char par_output_path[MAX_PATH];

                    sprintf_s(input_path, "%s\\%s", input_dir, findData.cFileName);
                    sprintf_s(seq_output_path, "%s\\seq_%.*s.qoi", output_dir, (int)(ext - findData.cFileName), findData.cFileName);
                    sprintf_s(par_output_path, "%s\\par_%.*s.qoi", output_dir, (int)(ext - findData.cFileName), findData.cFileName);

                    printf("Processing: %s\n", findData.cFileName);

                    // Sequential encoding
                    PerformanceMetrics seq_metrics = encode_file(input_path, seq_output_path);

                    // Parallel encoding
                    PerformanceMetrics par_metrics = encode_file_parallel(input_path, par_output_path);

                    // Print comparison results
                    print_comparison_results(findData.cFileName, seq_metrics, par_metrics, num_threads);
                }
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    else if (strcmp(mode, "decode") == 0) {
        sprintf_s(search_path, "%s\\*.qoi", input_dir);
        HANDLE hFind = FindFirstFileA(search_path, &findData);
        if (hFind == INVALID_HANDLE_VALUE) {
            printf("No .qoi files found in input directory\n");
            return 1;
        }

        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char input_path[MAX_PATH];
                char seq_output_path[MAX_PATH];
                char par_output_path[MAX_PATH];

                sprintf_s(input_path, "%s\\%s", input_dir, findData.cFileName);
                sprintf_s(seq_output_path, "%s\\seq_%.*s.png", output_dir, (int)(strlen(findData.cFileName) - 4), findData.cFileName);
                sprintf_s(par_output_path, "%s\\par_%.*s.png", output_dir, (int)(strlen(findData.cFileName) - 4), findData.cFileName);

                printf("Processing: %s\n", findData.cFileName);

                // Sequential decoding
                PerformanceMetrics seq_metrics = decode_file(input_path, seq_output_path);

                // Parallel decoding
                PerformanceMetrics par_metrics = decode_file_parallel(input_path, par_output_path);

                // Print comparison results
                print_comparison_results(findData.cFileName, seq_metrics, par_metrics, num_threads);
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    else {
        printf("Invalid mode. Use 'encode' or 'decode'.\n");
        return 1;
    }

    return 0;
}
