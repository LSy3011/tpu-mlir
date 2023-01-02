//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Conversion/TopToTpu/LoweringBM1684X.h"

namespace tpu_mlir {
namespace bm1684x {

void SplitLowering::LoweringINT8(PatternRewriter &rewriter,
                                 top::SplitOp unpackOp, bool asymmetric) const {
  llvm_unreachable("Not Implemented");
}
void SplitLowering::LoweringINT4(PatternRewriter &rewriter, top::SplitOp op,
                                   bool asymmetric) const {
  LoweringINT8(rewriter, op, asymmetric);
}
void SplitLowering::LoweringF32(PatternRewriter &rewriter,
                                top::SplitOp unpackOp) const {
  llvm_unreachable("Not Implemented");
}

void SplitLowering::LoweringBF16(PatternRewriter &rewriter,
                                 top::SplitOp unpackOp) const {
  llvm_unreachable("Not Implemented");
}

void SplitLowering::LoweringF16(PatternRewriter &rewriter,
                                top::SplitOp unpackOp) const {
  llvm_unreachable("Not Implemented");
}

void SplitLowering::LoweringQuantized(PatternRewriter &rewriter,
                                      top::SplitOp op) const {
  std::vector<Type> new_types;
  for (auto out : op.getResults()) {
    new_types.push_back(out.getType());
  }
  rewriter.replaceOpWithNewOp<tpu::SplitOp>(
      op, new_types, ValueRange{op.getInput()}, op->getAttrs());
}

} // namespace bm1684x
} // namespace tpu_mlir
