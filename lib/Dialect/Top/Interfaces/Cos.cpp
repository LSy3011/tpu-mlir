//===----------------------------------------------------------------------===//
//

//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Support/MathUtils.h"

int64_t top::CosOp::getFLOPs() {
  return module::getNumElements(getOutput()) * 4;
}

LogicalResult top::CosOp::init(InferenceParameter &p) { return success(); }
void top::CosOp::deinit(InferenceParameter &p) {}

LogicalResult top::CosOp::inference(InferenceParameter &p) {
  auto num_element = module::getNumElements(getInput());
#pragma omp parallel for schedule(static, omp_schedule(num_element))
  for (int i = 0; i < num_element; ++i) {
    auto val = p.inputs[0][i];
    p.outputs[0][i] = std::cos(val);
  }
  return success();
}

void top::CosOp::shape_inference() { common_shape_inference(getOperation()); }
