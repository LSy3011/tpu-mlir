//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Conversion/TopToTpu/LoweringBM1684.h"

namespace tpu_mlir {
namespace bm1684 {

void MatMulLowering::LoweringF32(PatternRewriter &rewriter,
                                 top::MatMulOp op) const {
  lowering_common_f32<tpu::MatMulOp>(rewriter, op);
}

void MatMulLowering::LoweringINT8(PatternRewriter &rewriter, top::MatMulOp op,
                                  bool asymmetric) const {
  // refer quantize_convlike_layer_int8
  OpBuilder builder(op);
  std::vector<Value> operands;
  std::vector<NamedAttribute> attrs;
  auto p = op.parseParam();
  assert(p.batch == 1); // only for fullyconnected now
  auto th_output = module::getThreshold(op.getOutput());
  auto th_input = module::getThreshold(op.getInput());
  auto filterOp = cast<top::WeightOp>(op.getRight().getDefiningOp());
  auto filter_f32 = filterOp.read<float>();
  double filter_max = findMaxabs(filter_f32->data(), filter_f32->size());
  int rshift =
      calRightShiftNum(filter_max, th_input, th_output, BITS_INT8);
  rshift = rshift >= 0 ? rshift : 0;
  std::shared_ptr<std::vector<int16_t>> bias_int16;
  if (p.with_bias) {
    float bias_scale = 1.0 * (1 << rshift) * QMAX_INT8 / th_output;
    auto biasOp = cast<top::WeightOp>(op.getBias().getDefiningOp());
    auto bias_f32 = biasOp.read<float>();
    bias_int16 = std::make_shared<std::vector<int16_t>>(bias_f32->size());
    float overflow_ratio = quantizeToInt16(bias_f32->data(), bias_int16->data(),
                                           bias_f32->size(), bias_scale);

    while (overflow_ratio > 0.03 && rshift > 0) {
      rshift--;
      bias_scale = 1.0 * (1 << rshift) * QMAX_INT8 / th_output;
      overflow_ratio = quantizeToInt16(bias_f32->data(), bias_int16->data(),
                                       bias_f32->size(), bias_scale);
    }
  }
  attrs.push_back(
      rewriter.getNamedAttr("rshifts", rewriter.getI64ArrayAttr(rshift)));
  float scale = 1.0 * (1 << rshift) * th_input / th_output;
  auto filter_int8 = std::make_shared<std::vector<int8_t>>(filter_f32->size());
  quantizeToInt8(filter_f32->data(), filter_int8->data(), filter_f32->size(),
                 scale);
  auto filter_type = op.getRight().getType().cast<RankedTensorType>();
  auto new_type =
      RankedTensorType::get(filter_type.getShape(), rewriter.getI8Type());
  auto new_filter =
      top::WeightOp::create(op, "filter_int8", *filter_int8, new_type);
  operands.push_back(op.getInput());
  operands.push_back(new_filter);
  auto new_bias = op.getBias();
  if (p.with_bias) {
    auto bias_type = op.getBias().getType().cast<RankedTensorType>();
    auto new_type = RankedTensorType::get(bias_type.getShape(),
                                          rewriter.getIntegerType(16));
    new_bias = top::WeightOp::create(op, "bias_int16", *bias_int16, new_type);
  }
  operands.push_back(new_bias);
  for (auto &attr : op->getAttrs()) {
    attrs.push_back(attr);
  }
  auto newType = getQuantInt8Type(op.getOutput());
  rewriter.replaceOpWithNewOp<tpu::MatMulOp>(op, newType, operands, attrs);
}

} // namespace bm1684
} // namespace tpu_mlir
