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
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "lowering-frcn-detection"
namespace tpu_mlir {
namespace cv18xx {
void loweringFrcnDetection(PatternRewriter &rewriter, top::FrcnDetectionOp op) {
  auto o_shape = module::getShape(op.getOutput());
  // lowering to cpu op
  std::vector<NamedAttribute> attrs;
  std::vector<NamedAttribute> param;
  attrs.emplace_back(rewriter.getNamedAttr(
      "cpu_op_name", rewriter.getStringAttr("frcn_detection")));
  param.emplace_back(rewriter.getNamedAttr(
      "class_num", rewriter.getI32IntegerAttr(op.getClassNum())));
  param.emplace_back(rewriter.getNamedAttr(
      "keep_topk", rewriter.getI32IntegerAttr(op.getKeepTopk())));
  param.emplace_back(rewriter.getNamedAttr(
      "obj_threshold",
      rewriter.getF32FloatAttr(op.getObjThreshold().convertToDouble())));
  param.emplace_back(rewriter.getNamedAttr(
      "nms_threshold",
      rewriter.getF32FloatAttr(op.getNmsThreshold().convertToDouble())));
  attrs.emplace_back(
      rewriter.getNamedAttr("param", rewriter.getDictionaryAttr(param)));
  std::vector<Value> operands(op.getOperands().begin(), op.getOperands().end());
  mlir::Type new_type = getQuantFloatType(op.getOutput());
  rewriter.replaceOpWithNewOp<tpu::GenericCpuOp>(op, new_type, operands, attrs);
}

void FrcnDetectionLowering::LoweringINT8(PatternRewriter &rewriter,
                                         top::FrcnDetectionOp op,
                                         bool asymmetric) const {
  loweringFrcnDetection(rewriter, op);
}

void FrcnDetectionLowering::LoweringBF16(PatternRewriter &rewriter,
                                         top::FrcnDetectionOp op) const {
  loweringFrcnDetection(rewriter, op);
}
} // namespace cv18xx
} // namespace tpu_mlir
