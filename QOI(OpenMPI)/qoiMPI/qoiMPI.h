#ifndef QOI_H
#define QOI_H

#define _CRT_SECURE_NO_WARNINGS

#ifdef __cplusplus
extern "C" {
#endif



#define QOI_SRGB   0
#define QOI_LINEAR 1

	typedef struct {
		unsigned int width;
		unsigned int height;
		unsigned char channels;
		unsigned char colorspace;
	} qoi_desc;

#ifndef QOI_NO_STDIO


	int qoi_write(const char* filename, const void* data, const qoi_desc* desc);
	void* qoi_read(const char* filename, qoi_desc* desc, int channels);

#endif 
	void* qoi_encode(const void* data, const qoi_desc* desc, int* out_len);
	void* qoi_decode(const void* data, int size, qoi_desc* desc, int channels);


#ifdef __cplusplus
}
#endif
#endif /* QOI_H */


#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#ifndef QOI_MALLOC
#define QOI_MALLOC(sz) malloc(sz)
#define QOI_FREE(p)    free(p)
#endif
#ifndef QOI_ZEROARR
#define QOI_ZEROARR(a) memset((a),0,sizeof(a))
#endif

#define QOI_OP_INDEX  0x00 /* 00xxxxxx */
#define QOI_OP_DIFF   0x40 /* 01xxxxxx */
#define QOI_OP_LUMA   0x80 /* 10xxxxxx */
#define QOI_OP_RUN    0xc0 /* 11xxxxxx */
#define QOI_OP_RGB    0xfe /* 11111110 */
#define QOI_OP_RGBA   0xff /* 11111111 */

#define QOI_MASK_2    0xc0 /* 11000000 */

#define QOI_COLOR_HASH(C) (C.rgba.r*3 + C.rgba.g*5 + C.rgba.b*7 + C.rgba.a*11)
#define QOI_MAGIC \
	(((unsigned int)'q') << 24 | ((unsigned int)'o') << 16 | \
	 ((unsigned int)'i') <<  8 | ((unsigned int)'f'))
#define QOI_HEADER_SIZE 14


#define QOI_PIXELS_MAX ((unsigned int)400000000)

typedef union {
	struct { unsigned char r, g, b, a; } rgba;
	unsigned int v;
} qoi_rgba_t;

static const unsigned char qoi_padding[8] = { 0,0,0,0,0,0,0,1 };

static void qoi_write_32(unsigned char* bytes, int* p, unsigned int v) {
	bytes[(*p)++] = (0xff000000 & v) >> 24;
	bytes[(*p)++] = (0x00ff0000 & v) >> 16;
	bytes[(*p)++] = (0x0000ff00 & v) >> 8;
	bytes[(*p)++] = (0x000000ff & v);
}

static unsigned int qoi_read_32(const unsigned char* bytes, int* p) {
	unsigned int a = bytes[(*p)++];
	unsigned int b = bytes[(*p)++];
	unsigned int c = bytes[(*p)++];
	unsigned int d = bytes[(*p)++];
	return a << 24 | b << 16 | c << 8 | d;
}

void* qoi_encode(const void* data, const qoi_desc* desc, int* out_len) {
	int rank, numProcess;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &numProcess);
	int64_t start_time, used_time;
	const unsigned char* pixels = NULL;
	int px_len = desc->width * desc->height * desc->channels;
	qoi_rgba_t index[64];
	QOI_ZEROARR(index);
	int run = 0;
	int p = 0,headerSize;
	if (rank == 0)
	{
		pixels = (const unsigned char*)data;
	
	}
	
	int numberOfRGB = desc->width * desc->height;
	int chunk_size = (numberOfRGB/numProcess)*desc->channels;
	int start;
	if (rank < (numberOfRGB % numProcess)) {
		start = rank * chunk_size + rank* desc->channels;
		chunk_size += desc->channels; //+1 for remainder if have
	}
	else {
		start = rank * chunk_size + (numberOfRGB % numProcess)*desc->channels;
	}
	unsigned char* local_data = NULL;
	if (rank == 0) {
		local_data = (unsigned char*)QOI_MALLOC(chunk_size);

	}
	else {
		local_data = (unsigned char*)QOI_MALLOC(chunk_size + desc->channels); //add one for preious pixel
	}
	if (!local_data)
	{
		printf("failed to allocated local data , rank %d", rank);
		return 0;
	}

	int* send_counts = NULL;
	int* sendDispls = NULL;
	int recv_count;
	recv_count = chunk_size;

	if (rank == 0)
	{
		send_counts = (int*)malloc(numProcess * sizeof(int));
		sendDispls = (int*)malloc(numProcess * sizeof(int));
	}
	MPI_Gather(&start, 1, MPI_INT, sendDispls, 1, MPI_INT, 0, MPI_COMM_WORLD);

	MPI_Gather(&recv_count, 1, MPI_INT, send_counts, 1, MPI_INT, 0, MPI_COMM_WORLD);

	MPI_Scatterv(pixels, send_counts, sendDispls, MPI_UNSIGNED_CHAR,
		local_data, recv_count, MPI_UNSIGNED_CHAR,
		0, MPI_COMM_WORLD);



	if (rank == 0)
	{
		free(send_counts);
		free(sendDispls);
	}


	qoi_rgba_t px, px_prev;
	
		px_prev.rgba.r = 0;
		px_prev.rgba.g = 0;
		px_prev.rgba.b = 0;
		px_prev.rgba.a = 255;
	

	px = px_prev;


	unsigned char* local_bytes = (unsigned char*)QOI_MALLOC(chunk_size * (desc->channels + 1));//give an value for store the encoded output
	if (!local_bytes) {
		printf("failed to allocated local bytes");
		return NULL;
	}

	int* block_offsets = NULL; //an pointer to store the first offsets address in the local bytes array.
	
	if (rank == 0) //write header file to into rank0's local_bytes
	{
		qoi_write_32(local_bytes, &p, QOI_MAGIC);
		qoi_write_32(local_bytes, &p, desc->width);
		qoi_write_32(local_bytes, &p, desc->height);
		local_bytes[p++] = desc->channels;
		local_bytes[p++] = desc->colorspace;
		qoi_write_32(local_bytes, &p, numProcess); //store num of block value
		block_offsets = (int*)(local_bytes + p); //store the first offsets address in the local bytes array.
		p += numProcess * sizeof(int); //jump the header leave it blank first.
		headerSize = p;//store header size
	}


	//start encoding
	for (int px_pos = 0; px_pos < chunk_size; px_pos += desc->channels) {
		px.rgba.r = local_data[px_pos + 0];
		px.rgba.g = local_data[px_pos + 1];
		px.rgba.b = local_data[px_pos + 2];
		if (desc->channels == 4) {
			px.rgba.a = local_data[px_pos + 3];
		}

		if (px.v == px_prev.v) {
			run++;
			if (run == 62 || px_pos == chunk_size - desc->channels) {
				local_bytes[p++] = QOI_OP_RUN | (run - 1);
				run = 0;
			}
		}
		else {
			if (run > 0) {
				local_bytes[p++] = QOI_OP_RUN | (run - 1);
				run = 0;
			}

			int index_pos = QOI_COLOR_HASH(px) % 64;
			if (index[index_pos].v == px.v) {
				local_bytes[p++] = QOI_OP_INDEX | index_pos;
			}
			else {
				index[index_pos] = px;
				if (px.rgba.a == px_prev.rgba.a) {
					signed char vr = px.rgba.r - px_prev.rgba.r;
					signed char vg = px.rgba.g - px_prev.rgba.g;
					signed char vb = px.rgba.b - px_prev.rgba.b;
					signed char vg_r = vr - vg;
					signed char vg_b = vb - vg;

					if (vr > -3 && vr < 2 && vg > -3 && vg < 2 && vb > -3 && vb < 2) {
						local_bytes[p++] = QOI_OP_DIFF | (vr + 2) << 4 | (vg + 2) << 2 | (vb + 2);
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
		}
		px_prev = px;
	}

	unsigned char* all_bytes = NULL;
	int* recv_counts = NULL;// an array to store everyone p
	int* displs = NULL;//an array to store the start point to write in all bytes


	if (rank == 0) {
		recv_counts = (int*)malloc(numProcess * sizeof(int));
		displs = (int*)malloc(numProcess * sizeof(int));
	}

	MPI_Gather(&p, 1, MPI_INT, recv_counts, 1, MPI_INT, 0, MPI_COMM_WORLD);
	int total_size;
	if (rank == 0) {
		total_size = 0;
		for (int i = 0; i < numProcess; i++) {
			
			displs[i] = total_size;
			block_offsets[i] = displs[i];
			total_size += recv_counts[i];
		}
		all_bytes = (unsigned char*)QOI_MALLOC(total_size);
	}
	MPI_Gatherv(local_bytes, p, MPI_UNSIGNED_CHAR, all_bytes, recv_counts, displs, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
	QOI_FREE(local_bytes);

	if (rank == 0) {
		*out_len = total_size;
		free(recv_counts);
		free(displs);
		return all_bytes;
	}
	else {
		return NULL;
	}
	return 0;
}

void* qoi_decode(const void* data, int size, qoi_desc* desc, int channels) {
	unsigned char* bytes=NULL;
	unsigned int header_magic = 0;
	unsigned char* partialPixels = NULL;
	unsigned char* allPixels = NULL;
	qoi_rgba_t index[64];
	qoi_rgba_t px;
	int px_len, chunks_len, px_pos;
	int p = 0, run = 0;
	int segmentPieces = 0;
	int rank, num_process;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &num_process);

	
	if (rank == 0)
	{
		if (
			data == NULL || desc == NULL ||
			(channels != 0 && channels != 3 && channels != 4) ||
			size < QOI_HEADER_SIZE + (int)sizeof(qoi_padding)
			) {
			return NULL;
		}
		bytes = (unsigned char*)data;

	}


	int num_blocks = NULL;
	int* block_offsets=NULL;
	
	if (rank == 0)
	{
		header_magic = qoi_read_32(bytes, &p);
		desc->width = qoi_read_32(bytes, &p);
		desc->height = qoi_read_32(bytes, &p);
		desc->channels = bytes[p++];
		desc->colorspace = bytes[p++];
		num_blocks = qoi_read_32(bytes, &p);// for example 15
		block_offsets = (int*)(bytes + p);
		p += num_blocks * sizeof(int);//jump header
	}
	
	MPI_Bcast(&num_blocks, 1, MPI_INT, 0, MPI_COMM_WORLD);
	if(rank != 0)
	{
		block_offsets = (int*)malloc(num_blocks * sizeof(int));
		if (block_offsets == NULL) {
			printf("Memory allocation failed for block_offsets on rank %d\n", rank);
			return NULL;
		}

	}
	MPI_Bcast(&size, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(desc, sizeof(qoi_desc), MPI_BYTE, 0, MPI_COMM_WORLD);
	MPI_Bcast(block_offsets, num_blocks, MPI_INT, 0, MPI_COMM_WORLD);
	chunks_len = size - (int)sizeof(qoi_padding);
	int startoffset;
	//calculate segment pieces
	int start = 0;
	
		segmentPieces = num_blocks / num_process;
		if (rank < num_blocks % num_process)
		{
			start = segmentPieces * rank + rank;
			segmentPieces++;
			
		}
		else
		{
			start = segmentPieces * rank + (num_blocks % num_process);
		}
		startoffset = block_offsets[start];
	
	unsigned char* local_bytes = NULL;
	int recv_count=0;
	int* send_count = NULL;
	int* sendDpls = NULL;
	
	//need modify
	
	int end = 0;
	
	if (rank == 0) {
		send_count = (int*)malloc(num_process * sizeof(int));
		sendDpls = (int*)malloc(num_process * sizeof(int));
	}

	
		if ((num_process <= num_blocks && rank == num_process - 1) || (num_process > num_blocks && rank == num_blocks - 1)) {
			recv_count = chunks_len - block_offsets[start];
		}
		else {
			recv_count = block_offsets[start + segmentPieces] - block_offsets[start];
			//printf("receive count %d,rank %d\n",recv_count,rank );
			//return 0;
		}
	
	

	MPI_Gather(&startoffset, 1, MPI_INT, sendDpls, 1, MPI_INT, 0, MPI_COMM_WORLD);

	MPI_Gather(&recv_count, 1, MPI_INT, send_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
	



	local_bytes = (unsigned char*)malloc(recv_count);

	MPI_Scatterv(bytes, send_count, sendDpls, MPI_UNSIGNED_CHAR,local_bytes, recv_count, MPI_UNSIGNED_CHAR,0, MPI_COMM_WORLD);
	


	if (rank == 0) {
		free(send_count);
		free(sendDpls);
	}
	

	if (
		desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 ||
		//header_magic != QOI_MAGIC ||
		desc->height >= QOI_PIXELS_MAX / desc->width
		)
	{
		printf("desc is null\n");
		return NULL;
	}

	if (channels == 0) {
		channels = desc->channels;
	}

	px_len = desc->width * desc->height * channels;
	partialPixels = (unsigned char*)QOI_MALLOC(px_len);

	if (!partialPixels) {
		printf("Memory allocation failed on rank %d\n", rank);
		return NULL;
	}

	QOI_ZEROARR(index);
	px.rgba.r = 0;
	px.rgba.g = 0;
	px.rgba.b = 0;
	px.rgba.a = 255;

	//printf(" local bytes : %d, rank %d\n", local_bytes[recv_count-1], rank);
	

	
	if (rank != 0)
	{
		p = 0;
	}
	

	int partialPixelsSize = 0;


	for (px_pos = 0; px_pos < px_len; px_pos += channels)
	{

		if (run > 0) {
			run--;
		}
		else if (p < recv_count) {
			int b1 = local_bytes[p++];
			

			if (b1 == QOI_OP_RGB) {
				px.rgba.r = local_bytes[p++];
				px.rgba.g = local_bytes[p++];
				px.rgba.b = local_bytes[p++];
			}
			else if (b1 == QOI_OP_RGBA) {
				px.rgba.r = local_bytes[p++];
				px.rgba.g = local_bytes[p++];
				px.rgba.b = local_bytes[p++];
				px.rgba.a = local_bytes[p++];
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) {
				px = index[b1];
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {

				px.rgba.r += ((b1 >> 4) & 0x03) - 2;
				px.rgba.g += ((b1 >> 2) & 0x03) - 2;
				px.rgba.b += (b1 & 0x03) - 2;
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {

				int b2 = local_bytes[p++];
				int vg = (b1 & 0x3f) - 32;
				px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
				px.rgba.g += vg;
				px.rgba.b += vg - 8 + (b2 & 0x0f);
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
				run = (b1 & 0x3f);
			}

			index[QOI_COLOR_HASH(px) % 64] = px;
		}

		partialPixels[px_pos + 0] = px.rgba.r;
		partialPixels[px_pos + 1] = px.rgba.g;
		partialPixels[px_pos + 2] = px.rgba.b;
		partialPixelsSize += 3;

		if (channels == 4) {
			partialPixels[px_pos + 3] = px.rgba.a;
			partialPixelsSize++;
		}

		if (p >= recv_count && run == 0)
		{
			break;
		}
	}


	int* recv_counts = NULL;
	int* displs = NULL;


	if (rank == 0) {
		recv_counts = (int*)malloc(num_process * sizeof(int));
		displs = (int*)malloc(num_process * sizeof(int));
		if (!recv_counts)
		{
			printf("\n\n\nfailed to allocated recv count \n\n\n");
		}
		
	}


	int error = MPI_Gather(&partialPixelsSize, 1, MPI_INT, recv_counts, 1, MPI_INT, 0, MPI_COMM_WORLD);
	if (error != MPI_SUCCESS) {
		printf("MPI_Bcast failed on rank %d\n", rank);
		MPI_Abort(MPI_COMM_WORLD, error);
	}
	


	int total_size = 0;
	if (rank == 0) {

		for (int i = 0; i < num_process; i++) {
			displs[i] = total_size;
			total_size += recv_counts[i];
		}
		allPixels = (unsigned char*)QOI_MALLOC(px_len *2);
		//allPixels = { 0 };
		//memset(allPixels, 0, px_len);
	}



	int erro = MPI_Gatherv(partialPixels, partialPixelsSize, MPI_UNSIGNED_CHAR, allPixels, recv_counts, displs, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

	if (erro != MPI_SUCCESS) {
		printf("MPI_Bcast failed on rank %d\n", rank);
		MPI_Abort(MPI_COMM_WORLD, erro);
	}
	
	free(local_bytes);

	if (rank == 0) {

		free(recv_counts);
		free(displs);
		free(partialPixels);
		//free(bytes);
		return allPixels;

		return 0;
	}
	else
	{
		//return partialPixels;
		free(partialPixels);
		free(block_offsets);
		return NULL;
	}


	//MPI_Barrier(MPI_COMM_WORLD);
	return 0;
}

void* qoi_encode_modify_serial(const void* data, const qoi_desc* desc, int* out_len) {
	if (data == NULL || out_len == NULL || desc == NULL ||
		desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 ||
		desc->height >= QOI_PIXELS_MAX / desc->width) {
		return NULL;
	}

	const int BLOCK_HEIGHT = 64;
	int width = desc->width;
	int height = desc->height;
	int channels = desc->channels;
	int num_blocks = (height + BLOCK_HEIGHT - 1) / BLOCK_HEIGHT;

	// Calculate max size including block offset table
	int max_size = width * height * (channels + 1) +
		QOI_HEADER_SIZE + sizeof(qoi_padding) + (num_blocks * sizeof(int));

	unsigned char* bytes = (unsigned char*)QOI_MALLOC(max_size);
	if (!bytes) return NULL;

	// Write header
	int p = 0;
	qoi_write_32(bytes, &p, QOI_MAGIC);
	qoi_write_32(bytes, &p, width);
	qoi_write_32(bytes, &p, height);
	bytes[p++] = channels;
	bytes[p++] = desc->colorspace;
	qoi_write_32(bytes, &p, num_blocks);  // Write number of blocks

	// Reserve space for block offsets
	int* block_offsets = (int*)(bytes + p);
	p += num_blocks * sizeof(int);

	const unsigned char* pixels = (const unsigned char*)data;

	// Arrays to track block output positions and sizes
	int* block_sizes = (int*)QOI_MALLOC(num_blocks * sizeof(int));
	unsigned char** block_outputs = (unsigned char**)QOI_MALLOC(num_blocks * sizeof(unsigned char*));
	if (!block_sizes || !block_outputs) {
		QOI_FREE(bytes);
		QOI_FREE(block_sizes);
		QOI_FREE(block_outputs);
		return NULL;
	}

	for (int block = 0; block < num_blocks; block++) {
		int start_row = block * BLOCK_HEIGHT;
		int end_row = min(start_row + BLOCK_HEIGHT, height);
		int block_px_len = width * (end_row - start_row) * channels;
		

		qoi_rgba_t index[64] = { 0 };
		qoi_rgba_t px = { 0, 0, 0, 255 };
		qoi_rgba_t px_prev = { 0, 0, 0, 255 };
		int run = 0;
		int local_size = 0;

		unsigned char* local_buffer = (unsigned char*)QOI_MALLOC(block_px_len * 2);
		if (!local_buffer) continue;

		// Start each block with full pixel
		int first_px = (start_row * width) * channels;
		px.rgba.r = pixels[first_px + 0];
		px.rgba.g = pixels[first_px + 1];
		px.rgba.b = pixels[first_px + 2];
		if (channels == 4) {
			px.rgba.a = pixels[first_px + 3];
			local_buffer[local_size++] = QOI_OP_RGBA;
		}
		else {
			local_buffer[local_size++] = QOI_OP_RGB;
		}
		local_buffer[local_size++] = px.rgba.r;
		local_buffer[local_size++] = px.rgba.g;
		local_buffer[local_size++] = px.rgba.b;
		if (channels == 4) {
			local_buffer[local_size++] = px.rgba.a;
		}
		px_prev = px;

		for (int y = start_row; y < end_row; y++) {
			for (int x = 0; x < width; x++) {
				// Skip first pixel as already encoded
				if (y == start_row && x == 0) continue;

				int px_pos = (y * width + x) * channels;
				px.rgba.r = pixels[px_pos + 0];
				px.rgba.g = pixels[px_pos + 1];
				px.rgba.b = pixels[px_pos + 2];
				if (channels == 4) {
					px.rgba.a = pixels[px_pos + 3];
				}

				if (px.v == px_prev.v) {
					run++;
					if (run == 62 || (y == end_row - 1 && x == width - 1)) {
						local_buffer[local_size++] = QOI_OP_RUN | (run - 1);
						run = 0;
					}
				}
				else {
					if (run > 0) {
						local_buffer[local_size++] = QOI_OP_RUN | (run - 1);
						run = 0;
					}

					int index_pos = QOI_COLOR_HASH(px) % 64;
					if (index[index_pos].v == px.v) {
						local_buffer[local_size++] = QOI_OP_INDEX | index_pos;
					}
					else {
						index[index_pos] = px;
						if (px.rgba.a == px_prev.rgba.a) {
							signed char vr = px.rgba.r - px_prev.rgba.r;
							signed char vg = px.rgba.g - px_prev.rgba.g;
							signed char vb = px.rgba.b - px_prev.rgba.b;
							signed char vg_r = vr - vg;
							signed char vg_b = vb - vg;

							if (vr > -3 && vr < 2 && vg > -3 && vg < 2 && vb > -3 && vb < 2) {
								local_buffer[local_size++] = QOI_OP_DIFF |
									(vr + 2) << 4 | (vg + 2) << 2 | (vb + 2);
							}
							else if (vg_r > -9 && vg_r < 8 && vg > -33 && vg < 32 &&
								vg_b > -9 && vg_b < 8) {
								local_buffer[local_size++] = QOI_OP_LUMA | (vg + 32);
								local_buffer[local_size++] = (vg_r + 8) << 4 | (vg_b + 8);
							}
							else {
								local_buffer[local_size++] = QOI_OP_RGB;
								local_buffer[local_size++] = px.rgba.r;
								local_buffer[local_size++] = px.rgba.g;
								local_buffer[local_size++] = px.rgba.b;
							}
						}
						else {
							local_buffer[local_size++] = QOI_OP_RGBA;
							local_buffer[local_size++] = px.rgba.r;
							local_buffer[local_size++] = px.rgba.g;
							local_buffer[local_size++] = px.rgba.b;
							local_buffer[local_size++] = px.rgba.a;
						}
					}
				}
				px_prev = px;
			}
		}

		block_sizes[block] = local_size;
		block_outputs[block] = local_buffer;
	}

	// Write blocks and store their offsets
	int write_pos = p;
	for (int i = 0; i < num_blocks; i++) {
		block_offsets[i] = write_pos - p;  // Store relative offset
		memcpy(bytes + write_pos, block_outputs[i], block_sizes[i]);
		write_pos += block_sizes[i];
		QOI_FREE(block_outputs[i]);
	}

	// Write padding
	memcpy(bytes + write_pos, qoi_padding, sizeof(qoi_padding));
	write_pos += sizeof(qoi_padding);

	QOI_FREE(block_sizes);
	QOI_FREE(block_outputs);

	*out_len = write_pos;
	return bytes;
}

void* qoi_decode_modify_serial(const void* data, int size, qoi_desc* desc, int channels) {
	if (data == NULL || desc == NULL ||
		(channels != 0 && channels != 3 && channels != 4) ||
		size < QOI_HEADER_SIZE + (int)sizeof(qoi_padding)) {
		return NULL;
	}

	const unsigned char* bytes = (const unsigned char*)data;
	int p = 0;

	// Read header
	unsigned int header_magic = qoi_read_32(bytes, &p);
	desc->width = qoi_read_32(bytes, &p);
	desc->height = qoi_read_32(bytes, &p);
	desc->channels = bytes[p++];
	desc->colorspace = bytes[p++];

	// Validate header
	if (desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 ||
		header_magic != QOI_MAGIC ||
		desc->height >= QOI_PIXELS_MAX / desc->width) {
		return NULL;
	}

	if (channels == 0) {
		channels = desc->channels;
	}

	// Read number of blocks
	int num_blocks = qoi_read_32(bytes, &p); // 15 is number of block

	// Read block offsets table   //16 - 30 is offset
	const int* block_offsets = (const int*)(bytes + p); //get 16 only, then store in block_offsets (16 address) when need then p++, but this is used address to read
	p += num_blocks * sizeof(int); //skip the header (30)

	int width = desc->width;
	int height = desc->height;
	const int BLOCK_HEIGHT = 64;
	int px_len = width * height * channels;
	int chunks_len = size - (int)sizeof(qoi_padding);

	unsigned char* pixels = (unsigned char*)QOI_MALLOC(px_len);
	if (!pixels) {
		return NULL;
	}


	for (int block = 0; block < num_blocks; block++) {
		qoi_rgba_t index[64] = { 0 };
		qoi_rgba_t px = { 0, 0, 0, 255 };
		int run = 0;

		// Direct access to block data using offset
		int local_p = p + block_offsets[block]; // use array read 
		int start_row = block * BLOCK_HEIGHT;
		int end_row = min(start_row + BLOCK_HEIGHT, height);

		for (int y = start_row; y < end_row; y++) {
			for (int x = 0; x < width; x++) {
				int px_pos = (y * width + x) * channels;

				if (run > 0) {
					run--;
				}
				else if (local_p < chunks_len) {
					int b1 = bytes[local_p++];
					if (b1 == QOI_OP_RGB) {
						px.rgba.r = bytes[local_p++];
						px.rgba.g = bytes[local_p++];
						px.rgba.b = bytes[local_p++];
					}
					else if (b1 == QOI_OP_RGBA) {
						px.rgba.r = bytes[local_p++];
						px.rgba.g = bytes[local_p++];
						px.rgba.b = bytes[local_p++];
						px.rgba.a = bytes[local_p++];
					}
					else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) {
						px = index[b1];
					}
					else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
						px.rgba.r += ((b1 >> 4) & 0x03) - 2;
						px.rgba.g += ((b1 >> 2) & 0x03) - 2;
						px.rgba.b += (b1 & 0x03) - 2;
					}
					else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
						int b2 = bytes[local_p++];
						int vg = (b1 & 0x3f) - 32;
						px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
						px.rgba.g += vg;
						px.rgba.b += vg - 8 + (b2 & 0x0f);
					}
					else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
						run = (b1 & 0x3f);
					}
					index[QOI_COLOR_HASH(px) % 64] = px;
				}

				pixels[px_pos + 0] = px.rgba.r;
				pixels[px_pos + 1] = px.rgba.g;
				pixels[px_pos + 2] = px.rgba.b;
				if (channels == 4) {
					pixels[px_pos + 3] = px.rgba.a;
				}
			}
		}
	}
	return pixels;
}

void* qoi_encode_serial(const void* data, const qoi_desc* desc, int* out_len) {
	int i, max_size, p, run;
	int px_len, px_end, px_pos, channels;
	unsigned char* bytes;
	const unsigned char* pixels;
	qoi_rgba_t index[64];
	qoi_rgba_t px, px_prev;

	if (
		data == NULL || out_len == NULL || desc == NULL ||
		desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 ||
		desc->height >= QOI_PIXELS_MAX / desc->width
		) {
		return NULL;
	}

	max_size =
		desc->width * desc->height * (desc->channels + 1) +
		QOI_HEADER_SIZE + sizeof(qoi_padding);

	p = 0;
	bytes = (unsigned char*)QOI_MALLOC(max_size);
	if (!bytes) {
		return NULL;
	}

	qoi_write_32(bytes, &p, QOI_MAGIC);
	qoi_write_32(bytes, &p, desc->width);
	qoi_write_32(bytes, &p, desc->height);
	bytes[p++] = desc->channels;
	bytes[p++] = desc->colorspace;


	pixels = (const unsigned char*)data;

	QOI_ZEROARR(index);

	run = 0;
	px_prev.rgba.r = 0;
	px_prev.rgba.g = 0;
	px_prev.rgba.b = 0;
	px_prev.rgba.a = 255;
	px = px_prev;

	px_len = desc->width * desc->height * desc->channels;
	px_end = px_len - desc->channels;
	channels = desc->channels;


	/////////////////////////////////////////////////////////////////////////////////////
	for (px_pos = 0; px_pos < px_len; px_pos += channels) {
		px.rgba.r = pixels[px_pos + 0];
		px.rgba.g = pixels[px_pos + 1];
		px.rgba.b = pixels[px_pos + 2];

		if (channels == 4) {
			px.rgba.a = pixels[px_pos + 3];
		}

		if (px.v == px_prev.v) {
			run++;
			if (run == 62 || px_pos == px_end) {
				bytes[p++] = QOI_OP_RUN | (run - 1);
				run = 0;
			}
		}
		else {
			int index_pos;

			if (run > 0) {
				bytes[p++] = QOI_OP_RUN | (run - 1);
				run = 0;
			}

			index_pos = QOI_COLOR_HASH(px) % 64;

			if (index[index_pos].v == px.v) {
				bytes[p++] = QOI_OP_INDEX | index_pos;
			}
			else {
				index[index_pos] = px;

				if (px.rgba.a == px_prev.rgba.a) {
					signed char vr = px.rgba.r - px_prev.rgba.r;
					signed char vg = px.rgba.g - px_prev.rgba.g;
					signed char vb = px.rgba.b - px_prev.rgba.b;

					signed char vg_r = vr - vg;
					signed char vg_b = vb - vg;

					if (
						vr > -3 && vr < 2 &&
						vg > -3 && vg < 2 &&
						vb > -3 && vb < 2
						) {
						bytes[p++] = QOI_OP_DIFF | (vr + 2) << 4 | (vg + 2) << 2 | (vb + 2);
					}
					else if (
						vg_r > -9 && vg_r <  8 &&
						vg   > -33 && vg   < 32 &&
						vg_b >  -9 && vg_b < 8
						) {
						bytes[p++] = QOI_OP_LUMA | (vg + 32);
						bytes[p++] = (vg_r + 8) << 4 | (vg_b + 8);
					}
					else {
						bytes[p++] = QOI_OP_RGB;
						bytes[p++] = px.rgba.r;
						bytes[p++] = px.rgba.g;
						bytes[p++] = px.rgba.b;
					}
				}
				else {
					bytes[p++] = QOI_OP_RGBA;
					bytes[p++] = px.rgba.r;
					bytes[p++] = px.rgba.g;
					bytes[p++] = px.rgba.b;
					bytes[p++] = px.rgba.a;
				}
			}
		}
		px_prev = px;
	}

	for (i = 0; i < (int)sizeof(qoi_padding); i++) {
		bytes[p++] = qoi_padding[i];
	}

	*out_len = p;
	return bytes;
}


void* qoi_decode_serial(const void* data, int size, qoi_desc* desc, int channels) {
	const unsigned char* bytes;
	unsigned int header_magic;
	unsigned char* pixels;
	qoi_rgba_t index[64];
	qoi_rgba_t px;
	int px_len, chunks_len, px_pos;
	int p = 0, run = 0;

	if (
		data == NULL || desc == NULL ||
		(channels != 0 && channels != 3 && channels != 4) ||
		size < QOI_HEADER_SIZE + (int)sizeof(qoi_padding)
		) {
		return NULL;
	}

	bytes = (const unsigned char*)data;

	header_magic = qoi_read_32(bytes, &p);
	desc->width = qoi_read_32(bytes, &p);
	desc->height = qoi_read_32(bytes, &p);
	desc->channels = bytes[p++];
	desc->colorspace = bytes[p++];

	if (
		desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 ||
		header_magic != QOI_MAGIC ||
		desc->height >= QOI_PIXELS_MAX / desc->width
		) {
		return NULL;
	}

	if (channels == 0) {
		channels = desc->channels;
	}

	px_len = desc->width * desc->height * channels;
	pixels = (unsigned char*)QOI_MALLOC(px_len);
	if (!pixels) {
		return NULL;
	}

	QOI_ZEROARR(index);
	px.rgba.r = 0;
	px.rgba.g = 0;
	px.rgba.b = 0;
	px.rgba.a = 255;

	chunks_len = size - (int)sizeof(qoi_padding);
	for (px_pos = 0; px_pos < px_len; px_pos += channels) {
		if (run > 0) {
			run--;
		}
		else if (p < chunks_len) {
			int b1 = bytes[p++];

			if (b1 == QOI_OP_RGB) {
				px.rgba.r = bytes[p++];
				px.rgba.g = bytes[p++];
				px.rgba.b = bytes[p++];
			}
			else if (b1 == QOI_OP_RGBA) {
				px.rgba.r = bytes[p++];
				px.rgba.g = bytes[p++];
				px.rgba.b = bytes[p++];
				px.rgba.a = bytes[p++];
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) {
				px = index[b1];
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
				px.rgba.r += ((b1 >> 4) & 0x03) - 2;
				px.rgba.g += ((b1 >> 2) & 0x03) - 2;
				px.rgba.b += (b1 & 0x03) - 2;
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
				int b2 = bytes[p++];
				int vg = (b1 & 0x3f) - 32;
				px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
				px.rgba.g += vg;
				px.rgba.b += vg - 8 + (b2 & 0x0f);
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
				run = (b1 & 0x3f);
			}

			index[QOI_COLOR_HASH(px) % 64] = px;
		}

		pixels[px_pos + 0] = px.rgba.r;
		pixels[px_pos + 1] = px.rgba.g;
		pixels[px_pos + 2] = px.rgba.b;

		if (channels == 4) {
			pixels[px_pos + 3] = px.rgba.a;
		}
	}

	return pixels;
}







#ifndef QOI_NO_STDIO
#include <stdio.h>

int qoi_write(const char* filename, const void* data, const qoi_desc* desc) {
	FILE* f = fopen(filename, "wb");
	int size, err;
	void* encoded;

	if (!f) {
		return 0;
	}

	encoded = qoi_encode(data, desc, &size);
	if (!encoded) {
		fclose(f);
		return 0;
	}

	fwrite(encoded, 1, size, f);
	fflush(f);
	err = ferror(f);
	fclose(f);

	QOI_FREE(encoded);
	return err ? 0 : size;
}

void* qoi_read(const char* filename, qoi_desc* desc, int channels) {
	FILE* f = fopen(filename, "rb");
	int size, bytes_read;
	void* pixels, * data;

	if (!f) {
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	if (size <= 0 || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return NULL;
	}

	data = QOI_MALLOC(size);
	if (!data) {
		fclose(f);
		return NULL;
	}

	bytes_read = fread(data, 1, size, f);
	fclose(f);
	pixels = (bytes_read != size) ? NULL : qoi_decode(data, bytes_read, desc, channels);
	QOI_FREE(data);
	return pixels;
}

#endif /* QOI_NO_STDIO */
