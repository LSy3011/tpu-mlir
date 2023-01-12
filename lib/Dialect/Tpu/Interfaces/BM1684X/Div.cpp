//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//
#include "tpu_mlir/Dialect/Tpu/IR/TpuOps.h"
#include "tpu_mlir/Backend/BM168x/BM1684X.h"

#include "tpu_mlir/Support/Module.h"



using namespace tpu_mlir::backend;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

// =========================================
// GlobalGenInterface
// =========================================

// int8
void tpu::DivOp::codegen_global_bm1684x() {
  auto op = getOperation();
  auto input_spec = BM168x::get_input_spec(op);
  auto output_spec = BM168x::get_output_spec(op);
  bcbinary_common_spec_t spec;
  memset(&spec, 0, sizeof(bcbinary_common_spec_t));
  spec.binary_type = BINARY_DIV;
  spec.if_relu = (int)getDoRelu();
  spec.relu_upper_limit = getReluLimit().convertToDouble();
  spec.scale_A = 1;
  spec.scale_B = 1;
  spec.rshift_A = 0;
  spec.rshift_B = 0;
  BM168x::call_global_func("backend_api_eltbinary_global", &spec, sizeof(spec),
                           input_spec->data(), output_spec->data());
}

// ======================================
// Dynamic GlobalGenInterface
// ======================================
int64_t tpu::DivOp::dyn_codegen_global_bm1684x(void *buffer) {
  return 0;
}
