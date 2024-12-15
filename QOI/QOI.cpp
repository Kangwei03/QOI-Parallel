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

// Sequential encoder
void encode_file(const std::string& input_path, const std::string& output_path) {
    int64_t start_time, load_time, process_time, save_time;

    start_time = get_time_ns();
    int width, height, channels;
    unsigned char* data = stbi_load(input_path.c_str(), &width, &height, &channels, 0);
    load_time = get_time_ns() - start_time;

    if (!data) {
        printf("Failed to load image: %s\n", input_path.c_str());
        return;
    }

    qoi_desc desc = { width, height, channels, QOI_SRGB };

    start_time = get_time_ns();
    int encoded_size;
    void* encoded_data = qoi_encode(data, &desc, &encoded_size);
    process_time = get_time_ns() - start_time;

    if (!encoded_data) {
        printf("Failed to encode image: %s\n", input_path.c_str());
        stbi_image_free(data);
        return;
    }

    start_time = get_time_ns();
    FILE* f = fopen(output_path.c_str(), "wb");
    if (f) {
        fwrite(encoded_data, 1, encoded_size, f);
        fclose(f);
    }
    save_time = get_time_ns() - start_time;

    int original_size = width * height * channels;
    float compression_ratio = (1.0f - (float)encoded_size / original_size) * 100;

    printf("+==============================================================================+\n");
    printf("| SEQUENTIAL ENCODE: %-70s |\n", input_path.c_str());
    printf("+==============================================================================+\n");
    printf("| Image Info:                                                                   |\n");
    printf("|   Dimensions    | %4dx%-4d                                                    |\n", width, height);
    printf("|   Channels      | %-4d                                                        |\n", channels);
    printf("|   Original Size | %-10s                                                   |\n", format_size(original_size).c_str());
    printf("|   Encoded Size  | %-10s                                                   |\n", format_size(encoded_size).c_str());
    printf("|   Compression   | %6.2f%%                                                     |\n", compression_ratio);
    printf("+------------------------------------------------------------------------------+\n");
    printf("| Performance:                                                                  |\n");
    printf("|   Load Time     | %8s ms                                                  |\n", format_duration(load_time / 1e6).c_str());
    printf("|   Encode Time   | %8s ms                                                  |\n", format_duration(process_time / 1e6).c_str());
    printf("|   Save Time     | %8s ms                                                  |\n", format_duration(save_time / 1e6).c_str());
    printf("|   Total Time    | %8s ms                                                  |\n", format_duration((load_time + process_time + save_time) / 1e6).c_str());
    printf("+==============================================================================+\n\n");

    free(encoded_data);
    stbi_image_free(data);
}

// Sequential decoder
void decode_file(const std::string& input_path, const std::string& output_path) {
    int64_t start_time, load_time, process_time, save_time;

    start_time = get_time_ns();
    FILE* f = fopen(input_path.c_str(), "rb");
    if (!f) {
        printf("Failed to open QOI file: %s\n", input_path.c_str());
        return;
    }

    fseek(f, 0, SEEK_END);
    int file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    void* raw_data = malloc(file_size);
    fread(raw_data, 1, file_size, f);
    fclose(f);
    load_time = get_time_ns() - start_time;

    start_time = get_time_ns();
    qoi_desc desc;
    void* decoded_data = qoi_decode(raw_data, file_size, &desc, 0);
    free(raw_data);
    process_time = get_time_ns() - start_time;

    if (!decoded_data) {
        printf("Failed to decode QOI file: %s\n", input_path.c_str());
        return;
    }

    start_time = get_time_ns();
    int success = stbi_write_png(output_path.c_str(), desc.width, desc.height,
        desc.channels, decoded_data, desc.width * desc.channels);
    save_time = get_time_ns() - start_time;

    if (!success) {
        printf("Failed to write PNG file: %s\n", output_path.c_str());
        free(decoded_data);
        return;
    }

    int decoded_size = desc.width * desc.height * desc.channels;

    printf("+==============================================================================+\n");
    printf("| SEQUENTIAL DECODE: %-70s |\n", input_path.c_str());
    printf("+==============================================================================+\n");
    printf("| Image Info:                                                                   |\n");
    printf("|   Dimensions    | %4dx%-4d                                                    |\n", desc.width, desc.height);
    printf("|   Channels      | %-4d                                                        |\n", desc.channels);
    printf("|   QOI Size      | %-10s                                                   |\n", format_size(file_size).c_str());
    printf("|   Decoded Size  | %-10s                                                   |\n", format_size(decoded_size).c_str());
    printf("+------------------------------------------------------------------------------+\n");
    printf("| Performance:                                                                  |\n");
    printf("|   Load Time     | %8s ms                                                  |\n", format_duration(load_time / 1e6).c_str());
    printf("|   Decode Time   | %8s ms                                                  |\n", format_duration(process_time / 1e6).c_str());
    printf("|   Save Time     | %8s ms                                                  |\n", format_duration(save_time / 1e6).c_str());
    printf("|   Total Time    | %8s ms                                                  |\n", format_duration((load_time + process_time + save_time) / 1e6).c_str());
    printf("+==============================================================================+\n\n");

    free(decoded_data);
}

//Parallel Encoder
void parallel_encode_file(const std::string& input_path, const std::string& output_path, int num_threads) {
    try {
        int64_t start_time, load_time, process_time, save_time;

        // Load image
        start_time = get_time_ns();
        int width, height, channels;
        unsigned char* data = stbi_load(input_path.c_str(), &width, &height, &channels, 0);
        load_time = get_time_ns() - start_time;

        if (!data) {
            printf("Failed to load image: %s\n", input_path.c_str());
            return;
        }

        printf("Loaded image: %s (%dx%d, %d channels)\n", input_path.c_str(), width, height, channels);

        qoi_desc desc = { width, height, channels, QOI_SRGB };

        // Prepare for parallel processing
        start_time = get_time_ns();
        int rows_per_thread = height / num_threads;
        int remaining_rows = height % num_threads;

        int* encoded_sizes = new int[num_threads]();
        void** encoded_chunks = new void* [num_threads]();

#pragma omp parallel num_threads(num_threads)
        {
            int thread_id = omp_get_thread_num();
            int start_row = thread_id * rows_per_thread + (thread_id < remaining_rows ? thread_id : remaining_rows);
            int end_row = start_row + rows_per_thread + (thread_id < remaining_rows ? 1 : 0);

            int start_pixel = start_row * width;
            int end_pixel = end_row * width;
            int chunk_pixels = end_pixel - start_pixel;

            qoi_desc chunk_desc = {
                width,
                static_cast<unsigned int>(end_row - start_row),
                channels,
                QOI_SRGB
            };

            unsigned char* chunk_data = new unsigned char[chunk_pixels * channels];
            memcpy(chunk_data, data + (start_pixel * channels), chunk_pixels * channels);

            encoded_chunks[thread_id] = qoi_encode(chunk_data, &chunk_desc, &encoded_sizes[thread_id]);

            delete[] chunk_data;

            if (!encoded_chunks[thread_id]) {
                printf("Thread %d failed to encode chunk.\n", thread_id);
            }
            else {
                printf("Thread %d encoded chunk: %d bytes.\n", thread_id, encoded_sizes[thread_id]);
            }
        }

        process_time = get_time_ns() - start_time;

        // Combine chunks
        start_time = get_time_ns();
        int total_size = 14; // Initial header size
        for (int i = 0; i < num_threads; i++) {
            if (encoded_sizes[i] > 0) {
                total_size += encoded_sizes[i] - (i == 0 ? 0 : 14); // Skip headers except the first
            }
        }
        total_size += 8; // End marker size

        void* final_data = malloc(total_size);
        if (!final_data) {
            throw std::runtime_error("Failed to allocate memory for final data");
        }

        int offset = 0;
        for (int i = 0; i < num_threads; i++) {
            if (encoded_chunks[i] && encoded_sizes[i] > 0) {
                if (i == 0) {
                    memcpy((char*)final_data + offset, encoded_chunks[i], encoded_sizes[i] - 8);
                    offset += encoded_sizes[i] - 8;
                }
                else {
                    memcpy((char*)final_data + offset, (char*)encoded_chunks[i] + 14, encoded_sizes[i] - 22);
                    offset += encoded_sizes[i] - 22;
                }
            }
        }

        const unsigned char end_marker[8] = { 0, 0, 0, 0, 0, 0, 0, 1 };
        memcpy((char*)final_data + offset, end_marker, 8);

        FILE* f = fopen(output_path.c_str(), "wb");
        if (f) {
            fwrite(final_data, 1, total_size, f);
            fclose(f);
        }
        save_time = get_time_ns() - start_time;

        // Output statistics
        int original_size = width * height * channels;
        float compression_ratio = (1.0f - (float)total_size / original_size) * 100;

        printf("+==============================================================================+\n");
        printf("| PARALLEL ENCODE (%d threads): %-50s |\n", num_threads, input_path.c_str());
        printf("+==============================================================================+\n");
        printf("| Image Info:                                                                   |\n");
        printf("|   Dimensions    | %4dx%-4d                                                    |\n", width, height);
        printf("|   Channels      | %-4d                                                        |\n", channels);
        printf("|   Original Size | %-10s                                                   |\n", format_size(original_size).c_str());
        printf("|   Encoded Size  | %-10s                                                   |\n", format_size(total_size).c_str());
        printf("|   Compression   | %6.2f%%                                                     |\n", compression_ratio);
        printf("+------------------------------------------------------------------------------+\n");
        printf("| Performance:                                                                  |\n");
        printf("|   Load Time     | %8s ms                                                  |\n", format_duration(load_time / 1e6).c_str());
        printf("|   Encode Time   | %8s ms                                                  |\n", format_duration(process_time / 1e6).c_str());
        printf("|   Save Time     | %8s ms                                                  |\n", format_duration(save_time / 1e6).c_str());
        printf("|   Total Time    | %8s ms                                                  |\n", format_duration((load_time + process_time + save_time) / 1e6).c_str());
        printf("+==============================================================================+\n\n");

        // Cleanup
        for (int i = 0; i < num_threads; i++) {
            if (encoded_chunks[i]) free(encoded_chunks[i]);
        }
        delete[] encoded_sizes;
        delete[] encoded_chunks;
        free(final_data);
        stbi_image_free(data);
    }
    catch (const std::exception& e) {
        printf("Error in parallel encoding: %s\n", e.what());
    }
}



// Parallel decoder
void parallel_decode_file(const std::string& input_path, const std::string& output_path, int num_threads) {
    printf("\nStarting parallel decoding with %d threads\n", num_threads);
    printf("Input file: %s\n", input_path.c_str());

    int64_t start_time, load_time, process_time, save_time;

    // Load QOI file
    start_time = get_time_ns();
    FILE* f = fopen(input_path.c_str(), "rb");
    if (!f) {
        printf("Failed to open QOI file: %s\n", input_path.c_str());
        return;
    }

    fseek(f, 0, SEEK_END);
    int file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    printf("File size: %d bytes\n", file_size);

    void* raw_data = malloc(file_size);
    if (!raw_data) {
        printf("Failed to allocate memory for raw data\n");
        fclose(f);
        return;
    }

    size_t bytes_read = fread(raw_data, 1, file_size, f);
    fclose(f);

    if (bytes_read != file_size) {
        printf("Failed to read entire file. Read %zu of %d bytes\n", bytes_read, file_size);
        free(raw_data);
        return;
    }

    load_time = get_time_ns() - start_time;

    // Decode QOI data
    start_time = get_time_ns();
    qoi_desc desc;
    unsigned char* decoded_data = (unsigned char*)qoi_decode(raw_data, file_size, &desc, 0);
    free(raw_data);

    if (!decoded_data) {
        printf("Failed to decode QOI data\n");
        return;
    }

    printf("Image dimensions: %dx%d with %d channels\n", desc.width, desc.height, desc.channels);

    int total_pixels = desc.width * desc.height;
    int chunk_size = total_pixels / num_threads;

    // Create a temporary buffer for parallel processing
    unsigned char* processed_data = new unsigned char[total_pixels * desc.channels];
    memcpy(processed_data, decoded_data, total_pixels * desc.channels);

#pragma omp parallel num_threads(num_threads)
    {
        int thread_id = omp_get_thread_num();
        int start_idx = thread_id * chunk_size;
        int end_idx = (thread_id == num_threads - 1) ? total_pixels : start_idx + chunk_size;

        printf("Thread %d processing pixels %d to %d\n", thread_id, start_idx, end_idx);

        // Process chunk of pixels
        for (int i = start_idx; i < end_idx; i++) {
            int pixel_idx = i * desc.channels;
            // Copy processed pixels to output buffer
            memcpy(&processed_data[pixel_idx], &decoded_data[pixel_idx], desc.channels);
        }
    }

    // Copy processed data back to decoded buffer
    memcpy(decoded_data, processed_data, total_pixels * desc.channels);
    delete[] processed_data;

    process_time = get_time_ns() - start_time;

    // Save as PNG
    start_time = get_time_ns();
    printf("Saving to: %s\n", output_path.c_str());

    int success = stbi_write_png(output_path.c_str(), desc.width, desc.height,
        desc.channels, decoded_data, desc.width * desc.channels);
    save_time = get_time_ns() - start_time;

    if (!success) {
        printf("Failed to write PNG file: %s\n", output_path.c_str());
        free(decoded_data);
        return;
    }

    int decoded_size = desc.width * desc.height * desc.channels;

    printf("+==============================================================================+\n");
    printf("| PARALLEL DECODE (%d threads): %-50s |\n", num_threads, input_path.c_str());
    printf("+==============================================================================+\n");
    printf("| Image Info:                                                                   |\n");
    printf("|   Dimensions    | %4dx%-4d                                                    |\n", desc.width, desc.height);
    printf("|   Channels      | %-4d                                                        |\n", desc.channels);
    printf("|   QOI Size      | %-10s                                                   |\n", format_size(file_size).c_str());
    printf("|   Decoded Size  | %-10s                                                   |\n", format_size(decoded_size).c_str());
    printf("+------------------------------------------------------------------------------+\n");
    printf("| Performance:                                                                  |\n");
    printf("|   Load Time     | %8s ms                                                  |\n", format_duration(load_time / 1e6).c_str());
    printf("|   Decode Time   | %8s ms                                                  |\n", format_duration(process_time / 1e6).c_str());
    printf("|   Save Time     | %8s ms                                                  |\n", format_duration(save_time / 1e6).c_str());
    printf("|   Total Time    | %8s ms                                                  |\n", format_duration((load_time + process_time + save_time) / 1e6).c_str());
    printf("+==============================================================================+\n\n");

    free(decoded_data);
    printf("Finished decoding %s\n\n", input_path.c_str());
}

int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0); // Disable output buffering

    if (argc != 5) {
        printf("Usage: %s [encode|decode] <input_dir> <output_dir> <num_threads>\n", argv[0]);
        return 1;
    }

    const char* mode = argv[1];
    const char* input_dir = argv[2];
    const char* output_dir = argv[3];
    const int num_threads = atoi(argv[4]);

    printf("Processing Mode: %s\n", mode);
    printf("Input Directory: %s\n", input_dir);
    printf("Output Directory: %s\n", output_dir);
    printf("Number of Threads: %d\n\n", num_threads);

    CreateDirectoryA(output_dir, NULL);

    WIN32_FIND_DATAA findData;
    HANDLE hFind;
    char search_path[MAX_PATH];

    if (strcmp(mode, "encode") == 0) {
        sprintf_s(search_path, "%s\\*.*", input_dir);
        hFind = FindFirstFileA(search_path, &findData);
        if (hFind == INVALID_HANDLE_VALUE) {
            printf("No files found in input directory\n");
            return 1;
        }

        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char* ext = strrchr(findData.cFileName, '.');
                if (ext && (_stricmp(ext, ".png") == 0 || _stricmp(ext, ".jpg") == 0 || _stricmp(ext, ".jpeg") == 0)) {
                    char input_path[MAX_PATH];
                    char output_path[MAX_PATH];
                    sprintf_s(input_path, "%s\\%s", input_dir, findData.cFileName);

                    if (num_threads > 1) {
                        sprintf_s(output_path, "%s\\par_%.*s.qoi", output_dir, (int)(ext - findData.cFileName), findData.cFileName);
                        printf("Processing file with parallel encoding: %s\n", findData.cFileName);
                        parallel_encode_file(input_path, output_path, num_threads);
                    }
                    else {
                        sprintf_s(output_path, "%s\\seq_%.*s.qoi", output_dir, (int)(ext - findData.cFileName), findData.cFileName);
                        printf("Processing file with sequential encoding: %s\n", findData.cFileName);
                        encode_file(input_path, output_path);
                    }
                    printf("Finished processing %s\n\n", findData.cFileName);
                }
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    else if (strcmp(mode, "decode") == 0) {
        sprintf_s(search_path, "%s\\*.qoi", input_dir);
        hFind = FindFirstFileA(search_path, &findData);
        if (hFind == INVALID_HANDLE_VALUE) {
            printf("No .qoi files found in input directory\n");
            return 1;
        }

        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char input_path[MAX_PATH];
                char output_path[MAX_PATH];
                sprintf_s(input_path, "%s\\%s", input_dir, findData.cFileName);

                sprintf_s(output_path, "%s\\%.*s.png", output_dir,
                    (int)(strlen(findData.cFileName) - 4), findData.cFileName);

                // Automatically determine if sequential or parallel decode
                if (strncmp(findData.cFileName, "seq_", 4) == 0) {
                    // Sequential decoding
                    printf("Processing sequential file: %s\n", findData.cFileName);
                    decode_file(input_path, output_path);
                }
                else if (strncmp(findData.cFileName, "seq_", 4) == 0) {
                    // Parallel decoding
                    printf("Processing parallel file: %s\n", findData.cFileName);
                    parallel_decode_file(input_path, output_path, num_threads);
                }
                else {
                    // Unknown type
                    printf("Skipping unknown file type: %s\n", findData.cFileName);
                }

                printf("Finished processing %s\n\n", findData.cFileName);
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