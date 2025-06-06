//===----------------------------------------------------------------------===//
//

//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Dialect/Tpu/Transforms/Codegen/Dynamic/DynamicLayer.hpp"
using namespace tpu_mlir::backend;

// =========================================
// GlobalGenInterface
// =========================================

void tpu::GenericCpuOp::codegen_global_bm1684x() {
  UNREACHABLE_THIS("Not Implemented");
}

// ======================================
// Dynamic GlobalGenInterface
// ======================================
int64_t tpu::GenericCpuOp::dyn_codegen_global_bm1684x(void *buffer) {
  return 0;
}

int64_t tpu::GenericCpuOp::get_fw_type_bm1684x() { return FW_LAYER_UNKNOWN; }
