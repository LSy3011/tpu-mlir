//===----------------------------------------------------------------------===//
//
// Copyright (c) 2020-2030 by Sophgo Technologies Inc. All rights reserved.
//
// Licensed under the Apache License v2.0.
// See http://www.apache.org/licenses/LICENSE-2.0 for license information.
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Conversion/TopToTpu/LoweringCV18xx.h"
#include "tpu_mlir/Support/MathUtils.h"
#include "llvm/Support/Debug.h"

namespace tpu_mlir {
namespace cv18xx {

void MaxPoolWithMaskConvert(PatternRewriter &rewriter,
                            top::MaxPoolWithMaskOp &op) {
  auto kernel_shape = module::getI64Array(op.getKernelShape());
  assert(kernel_shape->size() == 2 &&
         kernel_shape->at(0) == kernel_shape->at(1));
  std::vector<NamedAttribute> attrs;
  for (auto &attr : op->getAttrs()) {
    attrs.emplace_back(attr);
  }

  // create max_pool op
  auto max_pool_op = rewriter.create<top::MaxPoolOp>(
      op->getLoc(), op.getOutput().getType().cast<RankedTensorType>(),
      ValueRange{op.getInput()}, attrs);
  rewriter.setInsertionPointAfter(max_pool_op);

  // create pool mask op
  attrs.clear();
  attrs.emplace_back(rewriter.getNamedAttr(
      "scale", rewriter.getI64IntegerAttr(kernel_shape->at(0))));
  std::string name = module::getName(op.getMask()).str() + "_convert";
  auto loc = NameLoc::get(rewriter.getStringAttr(name));
  auto input_shape = module::getShape(op.getInput());
  std::vector<int64_t> mask_shape = input_shape.vec();
  mask_shape[2] = align_up(mask_shape[2], kernel_shape->at(0));
  mask_shape[3] = align_up(mask_shape[3], kernel_shape->at(0));

  auto pool_mask_type =
      RankedTensorType::get(mask_shape, rewriter.getF32Type());
  auto pool_mask_op = rewriter.create<top::PoolMaskOp>(
      loc, pool_mask_type, ValueRange{op.getInput()}, attrs);
  op.getMask().replaceAllUsesWith(pool_mask_op.getOutput());
  rewriter.replaceOp(op, {max_pool_op, pool_mask_op});
}

void MaxPoolWithMaskLowering::LoweringINT8(PatternRewriter &rewriter,
                                           top::MaxPoolWithMaskOp op,
                                           bool asymmetric) const {
  MaxPoolWithMaskConvert(rewriter, op);
}

void MaxPoolWithMaskLowering::LoweringBF16(PatternRewriter &rewriter,
                                           top::MaxPoolWithMaskOp op) const {
  MaxPoolWithMaskConvert(rewriter, op);
}

} // namespace cv18xx
} // namespace tpu_mlir
