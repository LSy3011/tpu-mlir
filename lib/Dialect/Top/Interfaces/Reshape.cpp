//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Dialect/Top/IR/TopOps.h"
#include "tpu_mlir/Support/Dnnl/Dnnl.h"
#include "tpu_mlir/Support/Module.h"
#include "tpu_mlir/Support/MathUtils.h"



int64_t top::ReshapeOp::getFLOPs() { return 0; }

LogicalResult top::ReshapeOp::init(InferenceParameter &p) { return success(); }
void top::ReshapeOp::deinit(InferenceParameter &p) {}

LogicalResult top::ReshapeOp::inference(InferenceParameter &p) {
  auto num_elem = module::getNumElements(getOutput());
#pragma omp parallel for schedule(static, omp_schedule(num_elem))
  for (int64_t i = 0; i < num_elem; i++) {
    p.outputs[0][i] = p.inputs[0][i];
  }
  return success();
}
