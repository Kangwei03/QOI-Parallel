#include "cuda_runtime.h"
#include "./QOIKernel.cuh"
#include <device_launch_parameters.h>
#include "qoi.h"
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <algorithm>
#include <cooperative_groups.h>

#ifndef QOI_ZEROARR
#define QOI_ZEROARR(a) memset((a),0,sizeof(a))
#endif

static void qoi_write_32(unsigned char* bytes, int* p, unsigned int v) {
    bytes[(*p)++] = (0xff000000 & v) >> 24;
    bytes[(*p)++] = (0x00ff0000 & v) >> 16;
    bytes[(*p)++] = (0x0000ff00 & v) >> 8;
    bytes[(*p)++] = (0x000000ff & v);
}

#define QOI_OP_RUN   0xC0
#define QOI_MAGIC (((unsigned int)'q') << 24 | ((unsigned int)'o') << 16 | ((unsigned int)'i') << 8 | ((unsigned int)'f'))
#define QOI_OP_INDEX 0x00
#define QOI_OP_DIFF  0x40
#define QOI_OP_LUMA  0x80
#define QOI_OP_RGB   0xFE
#define QOI_OP_RGBA  0xFF
#define QOI_MASK_2   0xC0
#define QOI_COLOR_HASH(C) (((C.rgba.r * 3) + (C.rgba.g * 5) + (C.rgba.b * 7) + (C.rgba.a * 11)) % 64)

#ifndef QOI_HEADER_SIZE
#define QOI_HEADER_SIZE 14
#endif

#ifndef __builtin_bswap32
#define __builtin_bswap32(x) \
    ((((x) & 0xff000000u) >> 24) | \
     (((x) & 0x00ff0000u) >> 8) | \
     (((x) & 0x0000ff00u) << 8) | \
     (((x) & 0x000000ffu) << 24))
#endif

typedef union {
    struct { unsigned char r, g, b, a; } rgba;
    unsigned int v;
} qoi_rgba_t;

__device__ void atomicWriteUChar(unsigned char* addr, unsigned char value) {
    unsigned int* base_addr = (unsigned int*)(((size_t)addr) & ~3);
    unsigned int shift = ((size_t)addr & 3) * 8;
    unsigned int mask = 0xFF << shift;
    unsigned int old, assumed;

    old = *base_addr;
    do {
        assumed = old;
        old = atomicCAS(base_addr,
            assumed,
            (assumed & ~mask) | (value << shift));
    } while (assumed != old);
}

__global__ void qoi_encode_kernel(
    const unsigned char* data,
    unsigned char* output,
    int width,
    int height,
    int channels,
    int* total_size,
    unsigned char* shared_local_bytes,
    int* shared_num_bytes,
    int* total_chunks_size
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total_pixels = width * height;
    int p = 0;
    int num_process = 512;

    if (idx >= total_pixels) return;

    qoi_rgba_t index[64];

    QOI_ZEROARR(index);

    int pixels_per_thread = (total_pixels + num_process - 1) / num_process;
    int start_pixel = idx * pixels_per_thread;
    int end_pixel = min(start_pixel + pixels_per_thread, total_pixels);
    int start = start_pixel * channels;
    int end = min(end_pixel * channels, total_pixels * channels);
    int chunk_size = end - start;

    unsigned char* local_bytes;

    qoi_rgba_t px, px_prev;

    if (idx < num_process) {
        local_bytes = &shared_local_bytes[idx * chunk_size];

        if (chunk_size > 0) {
            if (idx == 0)
            {
                px_prev.rgba.r = 0;
                px_prev.rgba.g = 0;
                px_prev.rgba.b = 0;
                px_prev.rgba.a = 255;
            }
            else if (channels == 3) {
                px_prev.rgba.r = data[start - 3];
                px_prev.rgba.g = data[start - 2];
                px_prev.rgba.b = data[start - 1];
                px_prev.rgba.a = 255;
            }
            else {
                px_prev.rgba.r = data[start - 4];
                px_prev.rgba.g = data[start - 3];
                px_prev.rgba.b = data[start - 2];
                px_prev.rgba.a = data[start - 1];
            }
            px = px_prev;



            int run = 0;

            for (int px_pos = start; px_pos < end && px_pos + channels <= total_pixels * channels; px_pos += channels) {
                if (p + 5 >= chunk_size) break;  // Ensure space for largest possible encoding

                px.rgba.r = data[px_pos + 0];
                px.rgba.g = data[px_pos + 1];
                px.rgba.b = data[px_pos + 2];
                px.rgba.a = (channels == 4) ? data[px_pos + 3] : 255;

                if (px.v == px_prev.v) {
                    run++;
                    if (run == 62 || px_pos == end - channels) {
                        local_bytes[p++] = QOI_OP_RUN | (run - 1);
                        run = 0;
                    }
                }
                else {
                    if (run > 0) {
                        local_bytes[p++] = QOI_OP_RUN | (run - 1);
                        run = 0;
                    }

                    if (px.rgba.a == px_prev.rgba.a) {
                        signed char vr = px.rgba.r - px_prev.rgba.r;
                        signed char vg = px.rgba.g - px_prev.rgba.g;
                        signed char vb = px.rgba.b - px_prev.rgba.b;
                        signed char vg_r = vr - vg;
                        signed char vg_b = vb - vg;

                        if (vr > -3 && vr < 2 && vg > -3 && vg < 2 && vb > -3 && vb < 2) {
                            local_bytes[p++] = QOI_OP_DIFF | ((vr + 2) << 4) | ((vg + 2) << 2) | (vb + 2);
                        }
                        else if (vg_r > -9 && vg_r < 8 && vg > -33 && vg < 32 && vg_b > -9 && vg_b < 8) {
                            local_bytes[p++] = QOI_OP_LUMA | (vg + 32);
                            local_bytes[p++] = (vg_r + 8) << 4 | (vg_b + 8);
                        }
                        else {
                            local_bytes[p++] = QOI_OP_RGB;
                            local_bytes[p++] = px.rgba.r;
                            local_bytes[p++] = px.rgba.g;
                            local_bytes[p++] = px.rgba.b;
                        }
                    }
                    else {
                        local_bytes[p++] = QOI_OP_RGBA;
                        local_bytes[p++] = px.rgba.r;
                        local_bytes[p++] = px.rgba.g;
                        local_bytes[p++] = px.rgba.b;
                        local_bytes[p++] = px.rgba.a;
                    }
                }
                px_prev = px;
            }

            // Ensure run is terminated at chunk end
            if (run > 0) {
                local_bytes[p++] = QOI_OP_RUN | (run - 1);
            }
        }

        shared_num_bytes[idx] = p;
        atomicAdd(total_size, p);
    }


    __syncthreads();

    if (idx < num_process)
    {

        int start_write = 0;

        for (int i = 0; i < idx; i++)
        {
            start_write += shared_num_bytes[i];
        }

        // Write chunk to global memory using a single atomic operation
        for (int i = 0; i < p; i++) {
            atomicWriteUChar(&output[start_write + i], local_bytes[i]);
        }



    }


}


void* qoi_encode_cuda(const void* data, const qoi_desc* desc, int* out_len) {
    if (!data || !desc || !out_len) return NULL;

    int total_pixels = desc->width * desc->height;
    int data_size = total_pixels * desc->channels;

    // Device memory
    unsigned char* d_data, * d_output, * d_shared_local_bytes;
    int* d_chunk_positions, * d_total_chunks, * d_shared_num_bytes, * d_total_chunks_size;

    // Allocate device memory
    cudaMalloc(&d_data, data_size);
    cudaMalloc(&d_output, total_pixels * 5); // Max 5 bytes per pixel
    cudaMalloc(&d_chunk_positions, total_pixels * sizeof(int));
    cudaMalloc(&d_total_chunks, sizeof(int));
    cudaMalloc(&d_shared_local_bytes, sizeof(unsigned char) * data_size * 2);
    cudaMalloc(&d_shared_num_bytes, sizeof(int) * 512);
    cudaMalloc(&d_total_chunks_size, sizeof(int));


    // Copy input data to device
    cudaMemcpy(d_data, data, data_size, cudaMemcpyHostToDevice);
    cudaMemset(d_total_chunks, 0, sizeof(int));

    // Launch kernel
    int threadsPerBlock = 512;  // Multiple of warp size (32)
    int blocksPerGrid = (512 + threadsPerBlock - 1) / threadsPerBlock;

    qoi_encode_kernel << <blocksPerGrid, threadsPerBlock >> > (
        d_data,
        d_output,
        desc->width,
        desc->height,
        desc->channels,
        d_total_chunks,
        d_shared_local_bytes,
        d_shared_num_bytes,
        d_total_chunks_size
        );

    // Get total chunks
    int total_chunks;
    cudaMemcpy(&total_chunks, d_total_chunks, sizeof(int), cudaMemcpyDeviceToHost);

    // Allocate and create final output with header
    int header_size = QOI_HEADER_SIZE;
    *out_len = header_size + total_chunks + 8; // +8 for end marker
    //printf("total chunkssssss = %d", total_chunks);
    unsigned char* output = (unsigned char*)malloc(*out_len);

    // Write header
    int p = 0;
    qoi_write_32(output, &p, QOI_MAGIC);
    qoi_write_32(output, &p, desc->width);
    qoi_write_32(output, &p, desc->height);
    output[p++] = desc->channels;
    output[p++] = desc->colorspace;

    // Copy encoded data
    cudaMemcpy(output + p, d_output, total_chunks, cudaMemcpyDeviceToHost);
    p += total_chunks;

    // Add end marker
    memcpy(output + p, "\0\0\0\0\0\0\0\1", 8);

    // Cleanup
    cudaFree(d_data);
    cudaFree(d_output);
    cudaFree(d_chunk_positions);
    cudaFree(d_total_chunks);
    cudaFree(d_shared_local_bytes);
    cudaFree(d_shared_num_bytes);
    cudaFree(d_total_chunks_size);

    return output;
}

__global__ void decode_chunks_kernel(
    const unsigned char* encoded_data,
    unsigned char* decoded_data,
    int channels,
    int* px_len,
    int* d_p,
    int* size,
    unsigned char* shared_partial_pixel,
    int* shared_num_bytes
) {
    int px_pos;
    int run = 0;
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int p = 0;
    int num_threads = 16;
    qoi_rgba_t px = { 0, 0, 0, 255 };  // Initialize default values 

    int chunks_len = *size - 8;

    int pixels_per_thread = (*px_len + num_threads - 1) / num_threads;
    unsigned char* partial_pixel = &shared_partial_pixel[idx * pixels_per_thread * channels];

    int chunk_size = *size / num_threads;
    int start = chunk_size * idx;


        px = { 0, 0, 0, 255 };
    

    int end = start + chunk_size;
    if (end < chunks_len && idx == num_threads - 1)
    {
        end = chunks_len;
    }

    if (idx < num_threads)
    {

        if (idx == 0) {
            p = 0;  // Start from header offset for first thread
        }
        else {
            // Calculate approximate start position based on previous chunk
            int encoded_chunk_size = *size / num_threads;
            p = encoded_chunk_size * idx;

            // Back up to find a valid opcode
            while (p > 0 &&
                encoded_data[p] != QOI_OP_RGB &&
                encoded_data[p] != QOI_OP_RGBA) {
                p--;
            }
        }

        while (encoded_data[end] != QOI_OP_RGB && encoded_data[end] != QOI_OP_RGBA && idx != num_threads - 1)
        {
            end--;

        };



        int partialPixelsSize = 0;

        for (px_pos = 0; px_pos < *px_len; px_pos += channels)
        {

            if (run > 0) {
                run--;
            }
            else if (p < end) {
                int b1 = encoded_data[p++];

                if (b1 == QOI_OP_RGB) {
                    px.rgba.r = encoded_data[p++];
                    px.rgba.g = encoded_data[p++];
                    px.rgba.b = encoded_data[p++];
                }
                else if (b1 == QOI_OP_RGBA) {
                    px.rgba.r = encoded_data[p++];
                    px.rgba.g = encoded_data[p++];
                    px.rgba.b = encoded_data[p++];
                    px.rgba.a = encoded_data[p++];
                }
                else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {

                    px.rgba.r += ((b1 >> 4) & 0x03) - 2;
                    px.rgba.g += ((b1 >> 2) & 0x03) - 2;
                    px.rgba.b += (b1 & 0x03) - 2;
                }
                else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {

                    int b2 = encoded_data[p++];
                    int vg = (b1 & 0x3f) - 32;
                    px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
                    px.rgba.g += vg;
                    px.rgba.b += vg - 8 + (b2 & 0x0f);
                }
                else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
                    run = (b1 & 0x3f);
                }

            }

            partial_pixel[px_pos + 0] = px.rgba.r;
            partial_pixel[px_pos + 1] = px.rgba.g;
            partial_pixel[px_pos + 2] = px.rgba.b;
            partialPixelsSize += 3;

            if (channels == 4) {
                partial_pixel[px_pos + 3] = px.rgba.a;
                partialPixelsSize++;
            }

            if (p >= end && run == 0)
            {
                break;
            }
        }
        shared_num_bytes[idx] = partialPixelsSize;
    }

    __syncthreads();


    if (idx < num_threads)
    {

        int start_write = 0;

        for (int i = 0; i < idx; i++)
        {
            start_write += shared_num_bytes[i];
        }

        // Write chunk to global memory using a single atomic operation
        for (int i = 0; i < shared_num_bytes[idx]; i++) {
            atomicWriteUChar(&decoded_data[start_write + i], partial_pixel[i]);
        }



    }
}

void* qoi_decode_cuda(const void* data, int size, qoi_desc* desc, int channels) {
    if (!data || !desc || size < QOI_HEADER_SIZE + 8) {
        return NULL;
    }

    const unsigned char* bytes = (const unsigned char*)data;
    int p = 0;

    // Read header
    unsigned int magic = (bytes[p] << 24) | (bytes[p + 1] << 16) | (bytes[p + 2] << 8) | bytes[p + 3];
    p += 4;
    desc->width = (bytes[p] << 24) | (bytes[p + 1] << 16) | (bytes[p + 2] << 8) | bytes[p + 3];
    p += 4;
    desc->height = (bytes[p] << 24) | (bytes[p + 1] << 16) | (bytes[p + 2] << 8) | bytes[p + 3];
    p += 4;
    desc->channels = bytes[p++];
    desc->colorspace = bytes[p++];

    if (channels == 0) {
        channels = desc->channels;
    }

    // Validate header
    if (magic != QOI_MAGIC || desc->width == 0 || desc->height == 0 ||
        desc->channels < 3 || desc->channels > 4 || desc->colorspace > 1) {
        return NULL;
    }

    int px_len = desc->width * desc->height * channels;
    int encoded_size = size - QOI_HEADER_SIZE - 8;  // Subtract header and end marker

    // Allocate device memory
    unsigned char* d_encoded, * d_decoded, * d_partial_pixels;
    int* d_ps_len, * d_p, * d_size, * d_shared_num_bytes;

    cudaMalloc(&d_encoded, encoded_size);
    cudaMalloc(&d_decoded, px_len);
    cudaMalloc(&d_ps_len, sizeof(int));
    cudaMalloc(&d_p, sizeof(int));
    cudaMalloc(&d_size, sizeof(int));
    cudaMalloc(&d_partial_pixels, px_len * 5);
    cudaMalloc(&d_shared_num_bytes, sizeof(int) * 16);


    // Copy encoded data to device
    cudaMemcpy(d_encoded, bytes + QOI_HEADER_SIZE, encoded_size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_ps_len, &px_len, sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d_size, &size, sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d_p, &p, sizeof(int), cudaMemcpyHostToDevice);

    // Launch kernels
    int threadsPerBlock = 16;  // Match your num_threads
    int blocksPerGrid = (16 + threadsPerBlock - 1) / threadsPerBlock;

    decode_chunks_kernel << <blocksPerGrid, threadsPerBlock >> > (
        d_encoded,
        d_decoded,
        channels,
        d_ps_len,
        d_p,
        d_size,
        d_partial_pixels,
        d_shared_num_bytes
        );

    // Allocate and copy result
    unsigned char* result = (unsigned char*)malloc(px_len);
    cudaMemcpy(result, d_decoded, px_len, cudaMemcpyDeviceToHost);

    // Cleanup
    cudaFree(d_encoded);
    cudaFree(d_decoded);
    cudaFree(d_ps_len);
    cudaFree(d_p);
    cudaFree(d_size);
    cudaFree(d_partial_pixels);
    cudaFree(d_shared_num_bytes);

    return result;
}