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
void tpu::LoopOp::codegen_global_bm1684x() {
  llvm_unreachable("Only support dynamic codegen");
}

int64_t tpu::LoopOp::dyn_codegen_global_bm1684x(void *buffer) { return 0; }

int64_t tpu::LoopOp::get_fw_type_bm1684x() { return FW_LAYER_UNKNOWN; }
