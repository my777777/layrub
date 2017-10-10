#include "caffe/common.hpp"
#include "caffe/syncedmem.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe {
SyncedMemory::SyncedMemory()
  : cpu_ptr_(NULL), gpu_ptr_(NULL), size_(0), head_(UNINITIALIZED),
    own_cpu_data_(false), cpu_malloc_use_cuda_(false), own_gpu_data_(false) {
#ifndef CPU_ONLY
#ifdef DEBUG
  CUDA_CHECK(cudaGetDevice(&device_));
#endif
#endif
}

SyncedMemory::SyncedMemory(size_t size)
  : cpu_ptr_(NULL), gpu_ptr_(NULL), size_(size), head_(UNINITIALIZED),
    own_cpu_data_(false), cpu_malloc_use_cuda_(false), own_gpu_data_(false) {
#ifndef CPU_ONLY
#ifdef DEBUG
  CUDA_CHECK(cudaGetDevice(&device_));
#endif
#endif
}

SyncedMemory::~SyncedMemory() {
//	LOG(INFO)<<"~SyncedMemory()";
  check_device();
  if (cpu_ptr_ && own_cpu_data_) {
    CaffeFreeHost(cpu_ptr_, cpu_malloc_use_cuda_);
    ///////////////////////////////////////////////////////170722
    cpu_ptr_ = NULL;
    own_cpu_data_ = false;
    /////////////////////////////////////////////////////////////
  }

#ifndef CPU_ONLY
  if (gpu_ptr_ && own_gpu_data_) {
    CUDA_CHECK(cudaFree(gpu_ptr_));
    ///////////////////////////////////////////////////////170722
    gpu_ptr_ = NULL;
    own_gpu_data_ = false;
    /////////////////////////////////////////////////////////////
  }
#endif  // CPU_ONLY
  ///////////////////////////////////////////////170722
  head_ = UNINITIALIZED;
  /////////////////////////////////////////////////////
}

inline void SyncedMemory::to_cpu() {
  check_device();
  switch (head_) {
  case UNINITIALIZED:
    CaffeMallocHost(&cpu_ptr_, size_, &cpu_malloc_use_cuda_);
    caffe_memset(size_, 0, cpu_ptr_);
    head_ = HEAD_AT_CPU;
    own_cpu_data_ = true;
    break;
  case HEAD_AT_GPU:
#ifndef CPU_ONLY
    if (cpu_ptr_ == NULL) {
      CaffeMallocHost(&cpu_ptr_, size_, &cpu_malloc_use_cuda_);
      own_cpu_data_ = true;
    }
    caffe_gpu_memcpy(size_, gpu_ptr_, cpu_ptr_);
    head_ = SYNCED;
#else
    NO_GPU;
#endif
    break;
  case HEAD_AT_CPU:
  case SYNCED:
    break;
  }
}

inline void SyncedMemory::to_gpu() {
  check_device();
#ifndef CPU_ONLY
  switch (head_) {
  case UNINITIALIZED:
    CUDA_CHECK(cudaMalloc(&gpu_ptr_, size_));
    caffe_gpu_memset(size_, 0, gpu_ptr_);
    head_ = HEAD_AT_GPU;
    own_gpu_data_ = true;
    break;
  case HEAD_AT_CPU:
    if (gpu_ptr_ == NULL) {
      CUDA_CHECK(cudaMalloc(&gpu_ptr_, size_));
      own_gpu_data_ = true;
    }
    caffe_gpu_memcpy(size_, cpu_ptr_, gpu_ptr_);
    head_ = SYNCED;
    break;
  case HEAD_AT_GPU:
  case SYNCED:
    break;
  }
#else
  NO_GPU;
#endif
}

const void* SyncedMemory::cpu_data() {
  check_device();
  to_cpu();
  return (const void*)cpu_ptr_;
}

void SyncedMemory::set_cpu_data(void* data) {
  check_device();
  CHECK(data);
  if (own_cpu_data_) {
    CaffeFreeHost(cpu_ptr_, cpu_malloc_use_cuda_);
  }
  cpu_ptr_ = data;
  head_ = HEAD_AT_CPU;
  own_cpu_data_ = false;
}

const void* SyncedMemory::gpu_data() {
  check_device();
#ifndef CPU_ONLY
  to_gpu();
  return (const void*)gpu_ptr_;
#else
  NO_GPU;
  return NULL;
#endif
}

void SyncedMemory::set_gpu_data(void* data) {
  check_device();
#ifndef CPU_ONLY
  CHECK(data);
  if (own_gpu_data_) {
    CUDA_CHECK(cudaFree(gpu_ptr_));
  }
  gpu_ptr_ = data;
  head_ = HEAD_AT_GPU;
  own_gpu_data_ = false;
#else
  NO_GPU;
#endif
}

void* SyncedMemory::mutable_cpu_data() {
  check_device();
  to_cpu();
  head_ = HEAD_AT_CPU;
  return cpu_ptr_;
}

void* SyncedMemory::mutable_gpu_data() {
  check_device();
#ifndef CPU_ONLY
  to_gpu();
  head_ = HEAD_AT_GPU;
  return gpu_ptr_;
#else
  NO_GPU;
  return NULL;
#endif
}

#ifndef CPU_ONLY
void SyncedMemory::async_gpu_push(const cudaStream_t& stream) {
  check_device();
  CHECK(head_ == HEAD_AT_CPU);
  if (gpu_ptr_ == NULL) {
    CUDA_CHECK(cudaMalloc(&gpu_ptr_, size_));
    own_gpu_data_ = true;
  }
  const cudaMemcpyKind put = cudaMemcpyHostToDevice;
  CUDA_CHECK(cudaMemcpyAsync(gpu_ptr_, cpu_ptr_, size_, put, stream));
  // Assume caller will synchronize on the stream before use
  head_ = SYNCED;
}
#endif

void SyncedMemory::check_device() {
#ifndef CPU_ONLY
#ifdef DEBUG
  int device;
  cudaGetDevice(&device);
  CHECK(device == device_);
  if (gpu_ptr_ && own_gpu_data_) {
    cudaPointerAttributes attributes;
    CUDA_CHECK(cudaPointerGetAttributes(&attributes, gpu_ptr_));
    CHECK(attributes.device == device_);
  }
#endif
#endif
}

///////////////////////////////////////////////////////////////////
void SyncedMemory::resize(const size_t size) {
	if (size_ >= size)
		return;
	size_ = size;
	CHECK(head_ == UNINITIALIZED) << "head state error in resize()";
	CHECK(!cpu_ptr_) << "cpu_ptr state error in resize()";
	CHECK(!gpu_ptr_) << "gpu_ptr state error in resize()";
	CHECK(!own_cpu_data_) << "own_cpu_data state error in resize()";
	CHECK(!own_gpu_data_) << "own_gpu_data state error in resize()";
	CHECK(!cpu_malloc_use_cuda_)
											<< "cpu_malloc_use_cuda state error in resize()";
}

void* SyncedMemory::transfer_to_cpu(const cudaStream_t& stream, const size_t size,
		void* cpu_ptr) {
	CHECK(gpu_ptr_);
	CHECK(head_ == HEAD_AT_GPU) << "head_: " << head_;
	if (!cpu_ptr) {// if cpu_ptr is NULL, then malloc at host
		CaffeMallocHost(&cpu_ptr, size, &cpu_malloc_use_cuda_);
	}
	CUDA_CHECK(cudaMemcpyAsync(cpu_ptr, gpu_ptr_, size, cudaMemcpyDeviceToHost,stream));
//	CUDA_CHECK(cudaStreamSynchronize(stream));
	return cpu_ptr;
}

void SyncedMemory::transfer_to_gpu(const cudaStream_t& stream, const size_t size,
		const void* cpu_ptr) {
	CHECK(cpu_ptr);
	CHECK(gpu_ptr_);
	CUDA_CHECK(cudaMemcpyAsync(gpu_ptr_, cpu_ptr, size, cudaMemcpyHostToDevice,stream));
//	CUDA_CHECK(cudaStreamSynchronize(stream));
}
////////////////////////////////////////////////////////////////////////////////////////

}  // namespace caffe

