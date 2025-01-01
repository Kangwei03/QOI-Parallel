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
#include <iostream>
#pragma warning(disable:4996)


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define QOI_IMPLEMENTATION
#include "qoiMPI.h"
#include <mpi.h>



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

void encode_file(const std::string& input_path, const std::string& output_path_serial, const std::string& output_path_pararllel) {
    int64_t start_time, load_time, process_time, save_time, serialStartTime,serialProscessingTime;
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    int width = 0, height = 0, channels = 0;
    unsigned char* data = NULL;
    unsigned char* partialData = NULL;

    if (rank == 0)
    {
        start_time = get_time_ns();
        data = stbi_load(input_path.c_str(), &width, &height, &channels, 0);
        load_time = get_time_ns() - start_time;
        printf("load_time : %8s ms \n", format_duration(load_time / 1e6).c_str());
        if (!data) {
            printf("Failed to load image: %s\n", input_path.c_str());
            return;
        }
    }
    if (rank == 0)
    {
        start_time = get_time_ns();
    }

    qoi_desc desc = { width, height, channels, QOI_SRGB };
    MPI_Bcast(&desc, sizeof(qoi_desc), MPI_BYTE, 0, MPI_COMM_WORLD);
    int totalSize = desc.width * desc.height * desc.channels;
    int encoded_size;

    void* encoded_data = qoi_encode(data, &desc, &encoded_size);
    if (rank == 0)
    {
        process_time = get_time_ns() - start_time;
        if (!encoded_data) {
            printf("Failed to encode image: %s\n", input_path.c_str());
            stbi_image_free(data);
            return;
        }

        // Save image
       // start_time = get_time_ns();
        FILE* f = fopen(output_path_pararllel.c_str(), "wb");
        if (f) {
            fwrite(encoded_data, 1, encoded_size, f);
            fclose(f);
        }
        serialStartTime = get_time_ns();
    }
    void* serial_encode = NULL;
    if (rank == 0)
    {
         serial_encode = qoi_encode_modify_serial(data, &desc, &encoded_size);
         serialProscessingTime = get_time_ns() - serialStartTime;

         FILE* f = fopen(output_path_serial.c_str(), "wb");
         if (f) {
             fwrite(serial_encode, 1, encoded_size, f);
             fclose(f);
         }
    }
    if (rank == 0)
    {
       // save_time = get_time_ns() - start_time;

        // Calculate metrics
        int original_size = width * height * channels;
        float compression_ratio = (1.0f - (float)encoded_size / original_size) * 100;

        double time_ratio = static_cast<double>(serialProscessingTime) / static_cast<double>(process_time);

        //// Format output
        printf("+==============================================================================+\n");
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
        printf("|   Pararllel Encode Time   | %8s ms                                                  |\n", format_duration(process_time / 1e6).c_str());
        printf("|   Serial Encode Time   | %8s ms                                                  |\n", format_duration(serialProscessingTime / 1e6).c_str());
        printf("|   performance Gain   | %lf times                                                  |\n", time_ratio);
        printf("+==============================================================================+\n\n");

        std::string filename = "C:\\Users\\yangy\\source\\repos\\qoiMPI\\qoiMPI\\output.csv";
        bool fileExists = std::ifstream(filename).good();
        std::ofstream file(filename, std::ios::app);  // Open in append mode

        if (file.is_open()) {
            // Write header if file is new
            if (!fileExists) {
                file << "Operation,ImageName,ImageSize,ThreadCount,LoadTime,DecodeTime,SaveTime,PerformanceGain,"
                    << "ImageWidth,ImageHeight,TotalPixels,FileSize,Channels\n";
            }

            // Extract image name from path
            std::string imageName = input_path.substr(input_path.find_last_of("/\\") + 1);

            // Write data row
   /*         file << "Encode" << ","
                << imageName << ","
                << desc.width << "x" << desc.height << ","
                << "1" << ","
                << format_duration(load_time / 1e6) << ","
                << format_duration(serialProscessingTime / 1e6) << ","
                << format_duration(save_time / 1e6) << ","
                << time_ratio << ","
                << desc.width << ","
                << desc.height << ","
                << (desc.width * desc.height) << ","
                << original_size << ","
                << desc.channels << "\n";

            file.close();
            std::cout << "Data appended to CSV file for image: " << imageName << "\n";
        }
        else {
            std::cerr << "Failed to write to CSV file.\n";*/
        }

    }

    free(encoded_data);
    free(serial_encode);
    stbi_image_free(data);
    
}


void decode_file(const std::string& input_path_serial, const std::string& input_path_parallel, const std::string& output_path) {
    int64_t start_time, load_time, process_time, save_time, serialStartTime, serialProscessingTime;
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    start_time = get_time_ns();
    void* raw_data = NULL;
    int file_size = 0;
    if (rank == 0)
    {
        FILE* f = fopen(input_path_parallel.c_str(), "rb");
        if (!f) {
            printf("Failed to open QOI file: %s\n", input_path_parallel.c_str());
            return;
        }

        fseek(f, 0, SEEK_END);
        file_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        raw_data = malloc(file_size);
        fread(raw_data, 1, file_size, f);
        fclose(f);
    }
    load_time = get_time_ns() - start_time;

    // Decode QOI data
    if (rank == 0)
    {
        start_time = get_time_ns();
    }

    qoi_desc desc;
    void* decoded_data = qoi_decode(raw_data, file_size, &desc, 0);
    free(raw_data);
    void* serial_data = NULL;
    if (rank == 0)
    {
        process_time = get_time_ns() - start_time;
        if (!decoded_data) {
            printf("Failed to decode QOI file: %s\n", input_path_parallel.c_str());
            return;
        }

        
        FILE* f = fopen(input_path_serial.c_str(), "rb");
        if (!f) {
          printf("Failed to open QOI file: %s\n", input_path_serial.c_str());
          return;
        }

        fseek(f, 0, SEEK_END);
        file_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        raw_data = malloc(file_size);
        fread(raw_data, 1, file_size, f);
        fclose(f);

        serialStartTime = get_time_ns();
        serial_data = qoi_decode_modify_serial(raw_data, file_size, &desc, 0);
        free(raw_data);
        serialProscessingTime = get_time_ns() - serialStartTime;
    }


    // Save as PNG
  
    if (rank == 0)
    {
        start_time = get_time_ns();
        int success = stbi_write_png(output_path.c_str(), desc.width, desc.height,
            desc.channels, serial_data, desc.width * desc.channels);
        save_time = get_time_ns() - start_time;
        if (!success) {
            printf("Failed to write PNG file: %s\n", output_path.c_str());
            free(decoded_data);
            free(serial_data);
            return;
        }
        int decoded_size = desc.width * desc.height * desc.channels;
        double time_ratio = static_cast<double>(serialProscessingTime) / static_cast<double>(process_time);

        // Format output
        printf("+==============================================================================+\n");
        printf("| DECODE: %-70s |\n", input_path_serial.c_str());
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
        printf("|   performance Gain    | %lf ms                                                  |\n", time_ratio);
        printf("+==============================================================================+\n\n");

        // CSV file handling
        std::string filename = "C:\\Users\\yangy\\source\\repos\\qoiMPI\\qoiMPI\\output.csv";
        bool fileExists = std::ifstream(filename).good();
        std::ofstream file(filename, std::ios::app);  // Open in append mode

        if (file.is_open()) {
            // Write header if file is new
            if (!fileExists) {
                file << "Operation,ImageName,ImageSize,ThreadCount,LoadTime,DecodeTime,SaveTime,PerformanceGain,"
                    << "ImageWidth,ImageHeight,TotalPixels,FileSize,Channels\n";
            }

            // Extract image name from path
            std::string imageName = input_path_serial.substr(input_path_serial.find_last_of("/\\") + 1);

            // Write data row
            file << "Decode" << ","
                << imageName << ","
                << desc.width << "x" << desc.height << ","
                << size << ","
                << format_duration(load_time / 1e6) << ","
                << format_duration(process_time / 1e6) << ","
                << format_duration(save_time / 1e6) << ","
                << time_ratio << ","
                << desc.width << ","
                << desc.height << ","
                << (desc.width * desc.height) << ","
                << file_size<< ","
                << desc.channels << "\n";

            file.close();
            std::cout << "Data appended to CSV file for image: " << imageName << "\n";
        }
        else {
            std::cerr << "Failed to write to CSV file.\n";
        }
    }
    free(serial_data);
    free(decoded_data);
}

int main(int argc, char* argv[]) {
    if (argc < 4)
    {
        printf("please enter correct argv[] (program_dir.exe mode(encode/decode) input_dir output_dir)");
        return 0;
    }
   
    MPI_Init(&argc, &argv);
    int64_t start_time, used_time;
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // Get process rank
    MPI_Comm_size(MPI_COMM_WORLD, &size); // Get total number of 

    if (rank == 0)
    {
        start_time = get_time_ns();
    }

    const char * mode = argv[1];
    const char* input_dir = argv[2];
    const char* output_dir = argv[3];
   
    WIN32_FIND_DATAA findData;
    HANDLE hFind;
    char search_path[MAX_PATH];

    if (strcmp(mode, "encode") == 0) {
        std::string base_output_dir(output_dir);
        std::string serial_output_dir = base_output_dir + "\\serial";
        std::string parallel_output_dir = base_output_dir + "\\parallel";
        CreateDirectoryA(serial_output_dir.c_str(), NULL);
        CreateDirectoryA(parallel_output_dir.c_str(), NULL);


        sprintf_s(search_path, "%s\\*.*", input_dir);
        hFind = FindFirstFileA(search_path, &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    char* ext = strrchr(findData.cFileName, '.');
                    if (ext && (_stricmp(ext, ".png") == 0 || _stricmp(ext, ".jpg") == 0 || _stricmp(ext, ".jpeg") == 0)) {
                        char input_path[MAX_PATH] = {};
                        char output_path_serial[MAX_PATH] = {};
                        char output_path_parallel[MAX_PATH] = {};
                        sprintf_s(input_path, "%s\\%s", input_dir, findData.cFileName);
                        sprintf_s(output_path_serial, "%s\\%.*s.qoi", serial_output_dir.c_str(), (int)(ext - findData.cFileName), findData.cFileName);
                        sprintf_s(output_path_parallel, "%s\\%.*s.qoi", parallel_output_dir.c_str(), (int)(ext - findData.cFileName), findData.cFileName);
                        encode_file(input_path, output_path_serial, output_path_parallel);
                    }
                }

            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
    }
    else if (strcmp(mode, "decode") == 0) {
        std::string base_input_dir(input_dir);
        std::string serial_input_dir = base_input_dir + "\\serial";
        std::string parallel_input_dir = base_input_dir + "\\parallel";
      
        sprintf_s(search_path, "%s\\*.qoi", serial_input_dir.c_str());
        hFind = FindFirstFileA(search_path, &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                std::string file_name = findData.cFileName;
                std::string parallel_file_path = parallel_input_dir + "\\" + file_name;
                WIN32_FIND_DATAA parallelFindData;
                HANDLE hParallelFind = FindFirstFileA(parallel_file_path.c_str(), &parallelFindData);

                char input_path_serial[MAX_PATH];
                char input_path_pararllel[MAX_PATH];
                char output_path[MAX_PATH];
                if (rank == 0)
                {
                    sprintf_s(input_path_serial, "%s\\%s", serial_input_dir.c_str(), findData.cFileName);
                    sprintf_s(input_path_pararllel, "%s\\%s", parallel_input_dir.c_str(), findData.cFileName);
                    sprintf_s(output_path, "%s\\%.*s.png", output_dir, (int)(strlen(findData.cFileName) - 4), findData.cFileName);
                }
                decode_file(input_path_serial, input_path_pararllel , output_path);
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
    }
    else {
        printf("Invalid mode. Use 'encode' or 'decode'.\n");
        return 1;
    }

    if (rank == 0)
    {
        used_time = get_time_ns() - start_time;
        printf("used time : %8s ms \n", format_duration(used_time / 1e6).c_str());
    }



    MPI_Finalize();


    return 0;
}