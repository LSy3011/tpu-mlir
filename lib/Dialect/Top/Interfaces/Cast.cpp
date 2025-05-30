//===----------------------------------------------------------------------===//
//

//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Support/MathUtils.h"

int64_t top::CastOp::getFLOPs() { return module::getNumElements(getOutput()); }

LogicalResult top::CastOp::init(InferenceParameter &p) { return success(); }
void top::CastOp::deinit(InferenceParameter &p) {}

LogicalResult top::CastOp::inference(InferenceParameter &p) {
  UNREACHABLE_THIS("Not Implemented");
  return failure();
}

void top::CastOp::shape_inference() { common_shape_inference(getOperation()); }
