#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <Windows.h>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
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

void encode_file(const std::string& input_path, const std::string& output_path) {
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

    qoi_desc desc = { width, height, channels, QOI_SRGB };

    // Encode image
    start_time = get_time_ns();
    int encoded_size;
    void* encoded_data = qoi_encode(data, &desc, &encoded_size);
    process_time = get_time_ns() - start_time;

    if (!encoded_data) {
        printf("Failed to encode image: %s\n", input_path.c_str());
        stbi_image_free(data);
        return;
    }

    // Save image
    start_time = get_time_ns();
    FILE* f = fopen(output_path.c_str(), "wb");
    if (f) {
        fwrite(encoded_data, 1, encoded_size, f);
        fclose(f);
    }
    save_time = get_time_ns() - start_time;

    // Calculate metrics
    int original_size = width * height * channels;
    float compression_ratio = (1.0f - (float)encoded_size / original_size) * 100;

    // Format output
   /* printf("+==============================================================================+\n");
    printf("| ENCODE: %-70s |\n", input_path.c_str());
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
    printf("+==============================================================================+\n\n");*/

    free(encoded_data);
    stbi_image_free(data);
}

void decode_file(const std::string& input_path, const std::string& output_path) {
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

    void* raw_data = malloc(file_size);
    fread(raw_data, 1, file_size, f);
    fclose(f);
    load_time = get_time_ns() - start_time;

    // Decode QOI data
    start_time = get_time_ns();
    qoi_desc desc;
    void* decoded_data = qoi_decode(raw_data, file_size, &desc, 0);
    free(raw_data);
    process_time = get_time_ns() - start_time;

    if (!decoded_data) {
        printf("Failed to decode QOI file: %s\n", input_path.c_str());
        return;
    }

    // Save as PNG
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

    // Format output
    printf("+==============================================================================+\n");
    printf("| DECODE: %-70s |\n", input_path.c_str());
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

int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);  // Disable output buffering
    int64_t start_time, used_time;
  /*  if (argc != 4) {
        printf("Usage: %s [encode|decode] <input_dir> <output_dir>\n", argv[0]);
        return 1;
    }*/
    start_time = get_time_ns();

    const char* mode = "decode";
    const char* input_dir = "C:\\ZheYangBackup\\QOIoutput";
    const char* output_dir = "C:\\ZheYangBackup\\input";
    if (strcmp(mode, "encode") == 0)
    {
       input_dir = "C:\\ZheYangBackup\\input";
       output_dir = "C:\\ZheYangBackup\\QOIoutput";
    }
  
  

    // Create output directory if it doesn't exist
    CreateDirectoryA(output_dir, NULL);

    WIN32_FIND_DATAA findData;
    HANDLE hFind;
    char search_path[MAX_PATH];

    if (strcmp(mode, "encode") == 0) {
        sprintf_s(search_path, "%s\\*.*", input_dir);
        hFind = FindFirstFileA(search_path, &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    char* ext = strrchr(findData.cFileName, '.');
                    if (ext && (_stricmp(ext, ".png") == 0 || _stricmp(ext, ".jpg") == 0 || _stricmp(ext, ".jpeg") == 0)) {
                        char input_path[MAX_PATH];
                        char output_path[MAX_PATH];
                        sprintf_s(input_path, "%s\\%s", input_dir, findData.cFileName);
                        sprintf_s(output_path, "%s\\%.*s.qoi", output_dir, (int)(ext - findData.cFileName), findData.cFileName);
                        encode_file(input_path, output_path);
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
    }
    else if (strcmp(mode, "decode") == 0) {
        sprintf_s(search_path, "%s\\*.qoi", input_dir);
        hFind = FindFirstFileA(search_path, &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                char input_path[MAX_PATH];
                char output_path[MAX_PATH];
                sprintf_s(input_path, "%s\\%s", input_dir, findData.cFileName);
                sprintf_s(output_path, "%s\\%.*s.png", output_dir, (int)(strlen(findData.cFileName) - 4), findData.cFileName);
                decode_file(input_path, output_path);
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
    }
    else {
        printf("Invalid mode. Use 'encode' or 'decode'.\n");
        return 1;
    }

    used_time = get_time_ns() - start_time;
    printf("used time : %8s ms \n", format_duration(used_time / 1e6).c_str());
    return 0;
}