//===----------------------------------------------------------------------===//
//

//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Support/Module.h"

int64_t top::NormalizeOp::getFLOPs() {
  return module::getNumElements(getOutput()) * 2;
}

LogicalResult top::NormalizeOp::init(InferenceParameter &p) {
  return success();
}
void top::NormalizeOp::deinit(InferenceParameter &p) {}
LogicalResult top::NormalizeOp::inference(InferenceParameter &p) {
  UNREACHABLE_THIS("Not Implemented");
  return success();
}

void top::NormalizeOp::shape_inference() {
  common_shape_inference(getOperation());
}
