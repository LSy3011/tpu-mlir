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

#define DEBUG_TYPE "lowering-proposal"
namespace tpu_mlir {
namespace cv18xx {
void loweringProposal(PatternRewriter &rewriter, top::ProposalOp op) {
  auto o_shape = module::getShape(op.getOutput());
  // lowering to cpu op
  std::vector<NamedAttribute> attrs;
  std::vector<NamedAttribute> param;
  attrs.emplace_back(
      rewriter.getNamedAttr("cpu_op_name", rewriter.getStringAttr("proposal")));
  param.emplace_back(rewriter.getNamedAttr(
      "net_input_h", rewriter.getI32IntegerAttr(op.getNetInputH())));
  param.emplace_back(rewriter.getNamedAttr(
      "net_input_w", rewriter.getI32IntegerAttr(op.getNetInputW())));
  param.emplace_back(rewriter.getNamedAttr(
      "feat_stride", rewriter.getI32IntegerAttr(op.getFeatStride())));
  param.emplace_back(rewriter.getNamedAttr(
      "anchor_base_size", rewriter.getI32IntegerAttr(op.getAnchorBaseSize())));
  param.emplace_back(rewriter.getNamedAttr(
      "rpn_obj_threshold",
      rewriter.getF32FloatAttr(op.getRpnObjThreshold().convertToDouble())));
  param.emplace_back(rewriter.getNamedAttr(
      "rpn_nms_threshold",
      rewriter.getF32FloatAttr(op.getRpnNmsThreshold().convertToDouble())));
  param.emplace_back(rewriter.getNamedAttr(
      "rpn_nms_post_top_n",
      rewriter.getI32IntegerAttr(op.getRpnNmsPostTopN())));
  attrs.emplace_back(
      rewriter.getNamedAttr("param", rewriter.getDictionaryAttr(param)));
  std::vector<Value> operands(op.getOperands().begin(), op.getOperands().end());
  mlir::Type new_type = getQuantFloatType(op.getOutput());
  rewriter.replaceOpWithNewOp<tpu::GenericCpuOp>(op, new_type, operands, attrs);
}

void ProposalLowering::LoweringINT8(PatternRewriter &rewriter,
                                    top::ProposalOp op, bool asymmetric) const {
  loweringProposal(rewriter, op);
}

void ProposalLowering::LoweringBF16(PatternRewriter &rewriter,
                                    top::ProposalOp op) const {
  loweringProposal(rewriter, op);
}
} // namespace cv18xx
} // namespace tpu_mlir
