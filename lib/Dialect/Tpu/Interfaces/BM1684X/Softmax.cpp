//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Backend/BM168x/BM1684X.h"
#include "tpu_mlir/Dialect/Tpu/IR/TpuOps.h"
#include "tpu_mlir/Support/Module.h"




using namespace tpu_mlir::backend;

// =========================================
// GloballGenInterface
// =========================================
void tpu::SoftmaxOp::codegen_global_bm1684x() {
  auto op = getOperation();
  auto input_spec = BM168x::get_input_spec(op);
  auto output_spec = BM168x::get_output_spec(op);
  bool has_table = !getTable().getType().isa<NoneType>();
  float in_scale = 1.0;
  if (module::isUniformQuantized(getInput())) {
    auto in_qtype = module::getUniformQuantizedType(getInput());
    in_scale = in_qtype.getScale();
  }
  if (module::isUniformQuantized(getInput(), getOutput())) {
    if (getLog()) {
      llvm_unreachable("Not Implemented");
      return;
    }
    assert(has_table);
    auto out_qtype = module::getUniformQuantizedType(getOutput());
    softmax_tflite_fix8b_param_t param = {0};
    auto &common = param.common;
    common.begin_axis = getAxis();
    common.end_axis = getAxis();
    common.zero_point = out_qtype.getZeroPoint();
    common.scale_val = out_qtype.getScale();
    BM168x::call_global_func("backend_api_softmax_tflite_fix8b_global", &param,
                             sizeof(param), input_spec->data(),
                             output_spec->data());
  } else {
    softmax_global_param_t param = {0};
    auto &common = param.common;
    common.begin_axis = getAxis();
    common.end_axis = getAxis();
    common.scale_val = in_scale;
    common.log = getLog();
    BM168x::call_global_func("backend_api_softmax_global", &param,
                             sizeof(param), input_spec->data(),
                             output_spec->data());
  }
}

// ======================================
// Dynamic GlobalGenInterface
// ======================================
int64_t tpu::SoftmaxOp::dyn_codegen_global_bm1684x(void *buffer) {
  return 0;
}
