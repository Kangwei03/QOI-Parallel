#include "qoi.h"

void * qoi_encode_cuda(const void* data, const qoi_desc* desc, int* out_len);

void * qoi_decode_cuda(const void* data, int size, qoi_desc* desc, int channels);