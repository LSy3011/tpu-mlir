//===----------------------------------------------------------------------===//
//

//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Backend/BM168x/BM1684X.h"
#include "tpu_mlir/Dialect/Tpu/IR/TpuOps.h"
#include "tpu_mlir/Dialect/Tpu/Transforms/Codegen/Dynamic/DynamicLayer.hpp"
#include "tpu_mlir/Support/MathUtils.h"
#include "tpu_mlir/Support/Module.h"
using namespace tpu_mlir::backend;

void tpu::DepackRawOp::codegen_global_bm1684x() {
  auto op = getOperation();
  auto input_spec = BM168x::get_input_spec(op);
  auto output_spec = BM168x::get_output_spec(op);

  depack_raw_spec_t param = {0};
  param.pad[0] = getPaddingH();
  param.pad[1] = getPaddingW();
  param.white_level = getWhiteLevel().convertToDouble();
  param.black_level = getBlackLevel().convertToDouble();
  auto channel_order = module::getI64Array(getChannelOrder());
  param.start_point[0] = 0;
  param.start_point[1] = 0;
  for (int i = 0; i < 4; i++) {
    param.channel_order[i] = channel_order->at(i);
  }
  BM168x::call_global_func("backend_api_depack_raw_global", &param,
                           sizeof(param), input_spec->data(),
                           output_spec->data());
}

int64_t tpu::DepackRawOp::dyn_codegen_global_bm1684x(void *buffer) {
  if (!buffer)
    return sizeof(depack_raw_spec_t);
  depack_raw_spec_t param{0};
  param.pad[0] = 0;
  param.pad[1] = 0;
  param.white_level = 4095.f;
  param.black_level = 112.f;
  param.channel_order[0] = 1;
  param.channel_order[1] = 0;
  param.channel_order[2] = 2;
  param.channel_order[3] = 3;
  return BM168x::dynamic_spec_to_buffer(buffer, param);
}

int64_t tpu::DepackRawOp::get_fw_type_bm1684x() { return FW_LAYER_UNKNOWN; }
