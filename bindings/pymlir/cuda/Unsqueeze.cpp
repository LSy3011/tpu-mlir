//===----------------------------------------------------------------------===//
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "../pycuda.h"
#include "cuda_helper.h"

void py_cuda::cudaUnsqueezeOp(tpu::UnsqueezeOp op) {
  void *input = getCudaData(op.getInput());
  void *output = getCudaData(op.getOutput());
  auto size = module::getBytes(op.getOutput());
  CHECK_CUDA(cudaMemcpy(output, input, size, cudaMemcpyDeviceToDevice));
}
