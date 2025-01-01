#include <iostream>
#include "./QOIKernel.cuh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <Windows.h>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#pragma warning(disable:4996)

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define QOI_IMPLEMENTATION
#include "qoi.h"

// Improved timing function that returns time since epoch
int64_t get_time_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

// Helper function to format time duration
std::string format_duration(double milliseconds) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << milliseconds;
    return oss.str();
}

// Helper function to format file size
std::string format_size(int size) {
    if (size < 1024) return std::to_string(size) + " B";
    if (size < 1024 * 1024) return std::to_string(size / 1024) + " KB";
    return std::to_string(size / (1024 * 1024)) + " MB";
}


int main(int argc, char* argv[]) {

    if (argc < 4)
    {
        printf("Please enter the following format input to run the compress/decompress\n");
        printf("[program file] [encode\decode] [input directory] [output directory]\n");
        return 0;
    }

    const char* mode = argv[1];
    const char* input_dir = argv[2];
    const char* output_dir = argv[3];



    setvbuf(stdout, NULL, _IONBF, 0);  // Disable output buffering
    int64_t cuda_total_time = 0, cpu_total_time = 0;
    int64_t cuda_start, cpu_start;

    /*std::ofstream csv("C:\\Users\\jitya\\Desktop\\performance_data.csv");
    csv << "Operation,ImageSize,ThreadCount,ProcessingTime,ImageWidth,ImageHeight,TotalPixels,FileSize\n";*/

    //const char* mode = "decode";
    //const char* input_dir = "C:\\Users\\jitya\\Desktop\\qoi\\encode\\cuda";
    //const char* output_dir = "C:\\Users\\jitya\\Desktop\\qoi\\output";
    //if (strcmp(mode, "encode") == 0)
    //{
    //    input_dir = "C:\\Users\\jitya\\Desktop\\qoi\\images";
    //    output_dir = "C:\\Users\\jitya\\Desktop\\qoi\\encode";
    //}

    // Create output directory for CPU and CUDA results
    std::string cuda_output_dir = std::string(output_dir) + "\\cuda";
    std::string cpu_output_dir = std::string(output_dir) + "\\cpu";
    CreateDirectoryA(output_dir, NULL);
    CreateDirectoryA(cuda_output_dir.c_str(), NULL);
    CreateDirectoryA(cpu_output_dir.c_str(), NULL);

    WIN32_FIND_DATAA findData;
    HANDLE hFind;
    char search_path[MAX_PATH];

    if (strcmp(mode, "decode") == 0) {
        sprintf_s(search_path, "%s\\*.*", input_dir);
        hFind = FindFirstFileA(search_path, &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            int file_count = 0;
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    char* ext = strrchr(findData.cFileName, '.');
                    if (ext && _stricmp(ext, ".qoi") == 0) {
                        char input_path[MAX_PATH];
                        char cuda_output_path[MAX_PATH];
                        char cpu_output_path[MAX_PATH];

                        sprintf_s(input_path, "%s\\%s", input_dir, findData.cFileName);
                        sprintf_s(cuda_output_path, "%s\\%.*s.png", cuda_output_dir.c_str(), (int)(ext - findData.cFileName), findData.cFileName);
                        sprintf_s(cpu_output_path, "%s\\%.*s.png", cpu_output_dir.c_str(), (int)(ext - findData.cFileName), findData.cFileName);

                        // Load QOI file once
                        FILE* f = fopen(input_path, "rb");
                        if (!f) {
                            printf("Failed to open file: %s\n", input_path);
                            continue;
                        }

                        fseek(f, 0, SEEK_END);
                        int size = ftell(f);
                        fseek(f, 0, SEEK_SET);

                        void* file_data = malloc(size);
                        fread(file_data, 1, size, f);
                        fclose(f);

                        file_count++;
                        printf("\nProcessing file %d: %s\n", file_count, findData.cFileName);

                        // CUDA Decoding
                        {
                            printf("CUDA Decoding:\n");
                            qoi_desc desc;

                            

                            int64_t cuda_start = get_time_ns();
                            void* decoded_data = qoi_decode_cuda(file_data, size, &desc, 0);
                            int64_t cuda_time = get_time_ns() - cuda_start;
                            cuda_total_time += cuda_time;


                            //int thread_count = 8;
                            //csv << "Decode," << desc.width << 'x' << desc.height << "," << thread_count << ","
                            //    << format_duration(cuda_time / 1e6) << "," << desc.width << ","
                            //    << desc.height << "," << desc.width * desc.height << ","
                            //    << size << "\n";

                            // File I/O moved outside timing
                            if (decoded_data) {
                                stbi_write_png(cuda_output_path, desc.width, desc.height, desc.channels, decoded_data, 0);
                                free(decoded_data);
                            }

                            printf("CUDA Decode Time: %8s ms\n\n", format_duration(cuda_time / 1e6).c_str());
                        }

                        // CPU Decoding
                        {
                            printf("CPU Decoding:\n");
                            qoi_desc desc;


                            int64_t cpu_start = get_time_ns();
                            void* decoded_data = qoi_decode(file_data, size, &desc, 0);
                            int64_t cpu_time = get_time_ns() - cpu_start;
                            cpu_total_time += cpu_time;

                            // File I/O moved outside timing
                            if (decoded_data) {
                                stbi_write_png(cpu_output_path, desc.width, desc.height, desc.channels, decoded_data, 0);
                                free(decoded_data);
                            }

                            printf("CPU Decode Time: %8s ms\n\n", format_duration(cpu_time / 1e6).c_str());
                        }

                        free(file_data);
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);

            // Print final performance comparison
            printf("\n=== Decoding Performance Summary ===\n");
            printf("Total CUDA Decode Time: %8s ms\n", format_duration(cuda_total_time / 1e6).c_str());
            printf("Total CPU Decode Time:  %8s ms\n", format_duration(cpu_total_time / 1e6).c_str());
            printf("Decode Speedup: %.2fx\n", (double)cpu_total_time / cuda_total_time);

            if (file_count == 0) {
                printf("No .qoi files found in directory: %s\n", input_dir);
            }
        }
        else {
            printf("Failed to open directory: %s\n", input_dir);
        }
    }
    else if (strcmp(mode, "encode") == 0) {
        sprintf_s(search_path, "%s\\*.*", input_dir);
        hFind = FindFirstFileA(search_path, &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            int file_count = 0;
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    char* ext = strrchr(findData.cFileName, '.');
                    if (ext && (_stricmp(ext, ".png") == 0 || _stricmp(ext, ".jpg") == 0 || _stricmp(ext, ".jpeg") == 0)) {
                        char input_path[MAX_PATH];
                        char cuda_output_path[MAX_PATH];
                        char cpu_output_path[MAX_PATH];

                        sprintf_s(input_path, "%s\\%s", input_dir, findData.cFileName);
                        sprintf_s(cuda_output_path, "%s\\%.*s.qoi", cuda_output_dir.c_str(), (int)(ext - findData.cFileName), findData.cFileName);
                        sprintf_s(cpu_output_path, "%s\\%.*s.qoi", cpu_output_dir.c_str(), (int)(ext - findData.cFileName), findData.cFileName);

                        // Load image once
                        int width, height, channels;
                        unsigned char* data = stbi_load(input_path, &width, &height, &channels, 0);
                        if (!data) {
                            printf("Failed to load image: %s\n", input_path);
                            continue;
                        }

                        qoi_desc desc = { width, height, channels, QOI_SRGB };
                        file_count++;

                        printf("\nProcessing file %d: %s\n", file_count, findData.cFileName);
                        printf("Resolution: %dx%d, Channels: %d\n\n", width, height, channels);

                        // CUDA Encoding (measure only encoding time)
                        {
                            printf("CUDA Encoding:\n");
                            int encoded_size;

                            int64_t cuda_start = get_time_ns();
                            void* encoded_data = qoi_encode_cuda(data, &desc, &encoded_size);
                            int64_t cuda_time = get_time_ns() - cuda_start;
                            cuda_total_time += cuda_time;


                            //int thread_count = 8;

                            //csv << "Encode," << desc.width << 'x' << desc.height << "," << thread_count << ","
                            //    << format_duration(cuda_time / 1e6) << "," << desc.width << ","
                            //    << desc.height << "," << desc.width * desc.height << ","
                            //    << encoded_size << "\n";

                            // File I/O moved outside timing
                            if (encoded_data) {
                                FILE* f = fopen(cuda_output_path, "wb");
                                if (f) {
                                    fwrite(encoded_data, 1, encoded_size, f);
                                    fclose(f);
                                }
                                free(encoded_data);
                            }

                            printf("CUDA Encode Time: %8s ms\n\n", format_duration(cuda_time / 1e6).c_str());
                        }

                        // CPU Encoding (measure only encoding time)
                        {
                            printf("CPU Encoding:\n");
                            int encoded_size;


                            int64_t cpu_start = get_time_ns();
                            void* encoded_data = qoi_encode(data, &desc, &encoded_size);
                            int64_t cpu_time = get_time_ns() - cpu_start;
                            cpu_total_time += cpu_time;


                            // File I/O moved outside timing
                            if (encoded_data) {
                                FILE* f = fopen(cpu_output_path, "wb");
                                if (f) {
                                    fwrite(encoded_data, 1, encoded_size, f);
                                    fclose(f);
                                }
                                free(encoded_data);
                            }

                            printf("CPU Encode Time: %8s ms\n\n", format_duration(cpu_time / 1e6).c_str());
                        }

                        stbi_image_free(data);
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);

            // Print final performance comparison
            printf("\n=== Encoding Performance Summary ===\n");
            printf("Total CUDA Encode Time: %8s ms\n", format_duration(cuda_total_time / 1e6).c_str());
            printf("Total CPU Encode Time:  %8s ms\n", format_duration(cpu_total_time / 1e6).c_str());
            printf("Encode Speedup: %.2fx\n", (double)cpu_total_time / cuda_total_time);
        }
    }

    return 0;
}