//===----------------------------------------------------------------------===//
//
// Copyright (c) 2020-2030 by Sophgo Technologies Inc. All rights reserved.
//
// Licensed under the Apache License v2.0.
// See http://www.apache.org/licenses/LICENSE-2.0 for license information.
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Backend/CV18xx/CV18xx.h"
#include "tpu_mlir/Backend/CV18xx/CV18xx_global_api.h"
#include "tpu_mlir/Dialect/Tpu/IR/TpuOps.h"
#include "tpu_mlir/Support/Module.h"




using namespace tpu_mlir::backend;
// =========================================
// GlobalGenInterface
// =========================================

void tpu::LutBF16Op::codegen_global_cv18xx(int64_t layer_id) {

  int64_t n, c, h, w;
  module::getNCHW(getOutput(), n, c, h, w);
  gaddr_t ga_input = module::getAddress(getInput());
  gaddr_t ga_output = module::getAddress(getOutput());
  gaddr_t ga_table = module::getAddress(getTable());

  gaddr_t ga_mantissa = module::getAddress(getMantissa());
  auto _lut_mode = getLutMode();
  if (_lut_mode == LutBF16Mode::Slope) {
    cvi_backend_tg_bf16_lut_slope_kernel(
        layer_id, ga_input, ga_output, ga_table, ga_mantissa, n, c, h, w,
        getMinRange().convertToDouble(), getMaxRange().convertToDouble());
  } else if (_lut_mode == LutBF16Mode::Mantissa) {
    cvi_backend_tg_bf16_lut_mantissa_kernel(
        layer_id, ga_input, ga_output, ga_table, ga_mantissa, n, c, h, w, 0);
  } else if (_lut_mode == LutBF16Mode::Log) {
    cvi_backend_tg_bf16_lut_mantissa_kernel(
        layer_id, ga_input, ga_output, ga_table, ga_mantissa, n, c, h, w, 1);
  } else {
    llvm_unreachable("Not supported now!");
  }
}

// =========================================
// LocalGenInterface
// =========================================

int64_t tpu::LutBF16Op::getBufferSize_cv18xx(
    int64_t in_lmem_bytes, int64_t out_lmem_bytes, int64_t in_nslice,
    int64_t in_hslice, int64_t out_nslice, int64_t out_hslice) {
  llvm_unreachable("Not supported now");
}

void tpu::LutBF16Op::codegen_local_cv18xx(int64_t n_step, int64_t h_step) {
  llvm_unreachable("Not supported now");
}
