//===----------------------------------------------------------------------===//
//

//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Support/MathUtils.h"

int64_t top::LogOp::getFLOPs() {
  return module::getNumElements(getOutput()) * 4;
}

LogicalResult top::LogOp::init(InferenceParameter &p) { return success(); }
void top::LogOp::deinit(InferenceParameter &p) {}

LogicalResult top::LogOp::inference(InferenceParameter &p) {
  auto in_shape = module::getShape(getInput());
  module::setShape(getOutput(), in_shape);
  auto num_element = module::getNumElements(getInput());
#pragma omp parallel for schedule(static, omp_schedule(num_element))
  for (int i = 0; i < num_element; ++i) {
    auto val = p.inputs[0][i];
    p.outputs[0][i] = std::log(val);
  }
  return success();
}

void top::LogOp::shape_inference() { common_shape_inference(getOperation()); }
