//===- bm1690Dailect.cpp - BM1690 dialect  --------------------------------===//
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu-mlir/Dialect/BM1690/IR/BM1690.h"

using namespace mlir;
using namespace tpu_mlir::bm1690;

#include "tpu-mlir/Dialect/BM1690/IR/BM1690Dialect.cpp.inc"

void BM1690Dialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "tpu-mlir/Dialect/BM1690/IR/BM1690Ops.cpp.inc"
      >();
  registerTypes();
}
