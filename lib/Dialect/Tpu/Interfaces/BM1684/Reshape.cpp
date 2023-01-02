//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Dialect/Tpu/IR/TpuOps.h"
#include "tpu_mlir/Backend/BM168x/BM1684.h"

#include "tpu_mlir/Support/Module.h"
#include "tpu_mlir/Support/MathUtils.h"



using namespace tpu_mlir::backend;

void tpu::ReshapeOp::codegen_global_bm1684() {
  auto in_addr = module::getAddress(getInput());
  auto out_addr = module::getAddress(getOutput());
  if (in_addr == out_addr) {
    return;
  }
  int64_t in, ic, ih, iw, on, oc, oh, ow;
  module::getNCHW(getInput(), in, ic, ih, iw);
  module::getNCHW(getOutput(), on, oc, oh, ow);
  if (on != in) {
    llvm_unreachable("Not Implemented");
  } else {
    int total_num = align_up(on, 4l) * oc * oh * ow;
    BM1684::instance().dl_nodechip_global_memcpy_ex(
        in_addr, out_addr, 1, total_num, total_num, DTYPE_FP32, DTYPE_FP32,
        total_num, (CMD_ID_NODE *)BM1684::instance().cmdid_node);
  }
}
