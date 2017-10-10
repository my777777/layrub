#include <algorithm>
#include <vector>

#include "caffe/layers/relu_layer.hpp"

namespace caffe {

template <typename Dtype>
void ReLULayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  const Dtype* bottom_data = bottom[0]->cpu_data();
  Dtype* top_data = top[0]->mutable_cpu_data();
  const int count = bottom[0]->count();
  Dtype negative_slope = this->layer_param_.relu_param().negative_slope();
  for (int i = 0; i < count; ++i) {
    top_data[i] = std::max(bottom_data[i], Dtype(0))
        + negative_slope * std::min(bottom_data[i], Dtype(0));
  }
}

template <typename Dtype>
void ReLULayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down,
    const vector<Blob<Dtype>*>& bottom) {
  if (propagate_down[0]) {
    const Dtype* bottom_data = bottom[0]->cpu_data();
    const Dtype* top_diff = top[0]->cpu_diff();
    Dtype* bottom_diff = bottom[0]->mutable_cpu_diff();
    const int count = bottom[0]->count();
    Dtype negative_slope = this->layer_param_.relu_param().negative_slope();
    for (int i = 0; i < count; ++i) {
      bottom_diff[i] = top_diff[i] * ((bottom_data[i] > 0)
          + negative_slope * (bottom_data[i] <= 0));
    }
  }
}

////////////////////////////////////////////////////////////////////170728
template <typename Dtype>
void ReLULayer<Dtype>::TransferDataToCPU(const cudaStream_t& stream, int count){
	CHECK(char_bottom_data);
	/*while(cudaStreamQuery(0) != cudaSuccess){//170929
		LOG(INFO)<<"fw compute cause delay, layer relu";
	}*/
	CUDA_CHECK(cudaStreamSynchronize(0));//ensure the char_bottom_data's computation completed.
//	LOG(INFO)<<"Device Synchronized before transfer to cpu...";
	char_data_cpu_ptr_ = char_bottom_data->transfer_to_cpu(stream, count*sizeof(char), char_data_cpu_ptr_);
}

template <typename Dtype>
void ReLULayer<Dtype>::TransferDataToGPU(const cudaStream_t& stream, int count){
	CHECK(char_bottom_data);
	CHECK(char_data_cpu_ptr_);
	char_bottom_data->transfer_to_gpu(stream, count*sizeof(char), char_data_cpu_ptr_);
}

template <typename Dtype>
void ReLULayer<Dtype>::SetCharBottomDataTo(shared_ptr<SyncedMemory> shared){
	char_bottom_data = shared;
}
//////////////////////////////////////////////////////////////////////////

#ifdef CPU_ONLY
STUB_GPU(ReLULayer);
#endif

INSTANTIATE_CLASS(ReLULayer);

}  // namespace caffe
