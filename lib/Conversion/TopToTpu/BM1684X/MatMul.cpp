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

void MatMulLowering::LoweringF32(PatternRewriter &rewriter,
                                 top::MatMulOp op) const {
  lowering_common_f32<tpu::MatMulOp>(rewriter, op);
}

void MatMulLowering::LoweringINT8(PatternRewriter &rewriter, top::MatMulOp op,
                                  bool asymmetric) const {
  // refer quantize_convlike_layer_int8
  std::vector<Value> operands;
  std::vector<NamedAttribute> attrs;
  auto p = op.parseParam();
  int scale = 1, shift = 0;
  if (p.batch > 1 && p.with_bias != 0) {
    auto bias_size = module::getNumElements(op.getBias());
    if (bias_size > p.N)
      llvm_unreachable("BatchMatMul does not support batch-bias yet.");
  }
  if (auto filterOp = dyn_cast<top::WeightOp>(op.getRight().getDefiningOp())) {
    auto filter_f32 = filterOp.read<float>();
    int64_t in_zp = 0, out_zp = 0;
    double in_scale = 1, out_scale = 1, w_scale = 1;
    module::getScaleAndZeroPoint(op.getInput(), in_scale, in_zp, asymmetric);
    module::getScaleAndZeroPoint(op.getOutput(), out_scale, out_zp, asymmetric);
    if (p.batch > 1 && in_zp != 0) { // Cannot merge zp to bias in BatchMatMul
      LoweringF32(rewriter, op);
      return;
    }
    f64_array_t weight_scale_v;
    if (filterOp.getScale().has_value() && weight_scale_v->size()) {
      weight_scale_v = module::getF64Array(filterOp.getScale().value());
      w_scale = weight_scale_v->data()[0];
    } else {
      double w_max = findMaxabs(filter_f32->data(), filter_f32->size());
      w_scale = w_max / 127.0;
    }

    auto filter_int8 =
        std::make_shared<std::vector<int8_t>>(filter_f32->size());
    for (uint64_t t = 0; t < filter_f32->size(); t++) {
      filter_int8->at(t) = to_int8(filter_f32->at(t) / w_scale);
    }

    i32_array_t bias_int32;
    std::shared_ptr<std::vector<float>> bias_fp32;
    if (p.with_bias) {
      auto biasOp = cast<top::WeightOp>(op.getBias().getDefiningOp());
      bias_fp32 = biasOp.read<float>();
      bias_int32 = std::make_shared<std::vector<int32_t>>(bias_fp32->size());
    } else if (in_zp) {
      bias_int32 = std::make_shared<std::vector<int32_t>>(p.N);
    }

    for (int j = 0; j < p.N; j++) { // vector [1xN]
      int64_t bias_w_xz = 0;
      for (int i = 0; i < p.K; i++) {
        bias_w_xz += (int64_t)filter_int8->at(i * p.N + j) * in_zp;
      }

      if (p.with_bias) {
        bias_int32->data()[j] =
            std::round(bias_fp32->at(j) / (w_scale * in_scale) - bias_w_xz);
      } else if (in_zp) {
        bias_int32->data()[j] = -bias_w_xz;
      }
    }
    bool with_bias = p.with_bias || in_zp != 0;
    float scale_f = in_scale * w_scale / out_scale;
    get_scale_and_shift(scale_f, scale, shift, 32);
    auto filter_type = op.getRight().getType().cast<RankedTensorType>();
    auto new_type =
        RankedTensorType::get(filter_type.getShape(), rewriter.getI8Type());
    auto new_filter =
        top::WeightOp::create(op, "filter_int8", *filter_int8, new_type);
    operands.push_back(op.getInput());
    operands.push_back(new_filter);
    auto new_bias = op.getBias();
    if (with_bias) {
      std::vector<int64_t> shape = {p.N};
      auto new_type = RankedTensorType::get(shape, rewriter.getI32Type());
      new_bias = top::WeightOp::create(op, "bias_int32", *bias_int32, new_type);
      operands.push_back(new_bias);
    } else {
      auto none = module::getNoneOp(op);
      operands.push_back(none);
    }
  } else if (asymmetric) {
    LoweringF32(rewriter, op);
    return;
  } else { // mutable tensor or MatMul
    int64_t in_zp = 0, w_zp = 0, out_zp = 0;
    double in_scale = 1, w_scale = 1, out_scale = 1;
    module::getScaleAndZeroPoint(op.getInput(), in_scale, in_zp, asymmetric);
    module::getScaleAndZeroPoint(op.getRight(), w_scale, w_zp, asymmetric);
    module::getScaleAndZeroPoint(op.getOutput(), out_scale, out_zp, asymmetric);
    float scale_f = in_scale * w_scale / out_scale;
    get_scale_and_shift(scale_f, scale, shift, 32);
    for (auto operand : op.getOperands())
      operands.push_back(operand);
    if (p.with_bias) {
      auto biasOp = cast<top::WeightOp>(op.getBias().getDefiningOp());
      auto bias_fp32 = biasOp.read<float>();
      int bias_n = bias_fp32->size();
      auto bias_int32 = std::make_shared<std::vector<int32_t>>(bias_n);
      for (int j = 0; j < bias_n; j++) {
        bias_int32->data()[j] =
            std::round(bias_fp32->at(j) / (w_scale * in_scale));
      }
      auto new_type = RankedTensorType::get({bias_n}, rewriter.getI32Type());
      auto new_bias =
          top::WeightOp::create(op, "bias_int32", *bias_int32, new_type);
      operands[2] = new_bias;
    }
  }
  attrs.push_back(
      rewriter.getNamedAttr("rshifts", rewriter.getI64ArrayAttr(shift)));
  attrs.push_back(
      rewriter.getNamedAttr("multipliers", rewriter.getI64ArrayAttr(scale)));

  for (auto &attr : op->getAttrs()) {
    attrs.push_back(attr);
  }
  auto newType = getQuantInt8Type(op.getOutput(), asymmetric);
  rewriter.replaceOpWithNewOp<tpu::MatMulOp>(op, newType, operands, attrs);
}

void MatMulLowering::LoweringINT4(PatternRewriter &rewriter, top::MatMulOp op,
                                  bool asymmetric) const {

  // refer quantize_convlike_layer_int8
  llvm::errs() << "start MatMul LoweringINT4, name:"
               << module::getName(op.getOperation()).str() << "\n";
  std::vector<Value> operands;
  std::vector<NamedAttribute> attrs;
  auto p = op.parseParam();
  int scale = 1, shift = 0;
  if (p.batch > 1 && p.with_bias != 0) {
    auto bias_size = module::getNumElements(op.getBias());
    if (bias_size > p.N)
      llvm_unreachable("BatchMatMul does not support batch-bias yet.");
  }
  double in_int8_scale;
  int64_t in_int8_zp;
  bool all_next_layer_is_int8 = true;
  bool all_next_layer_is_int4 = true;
  double out_int8_scale = 1;
  double out_int8_zp = 0;
  int64_t in_zp = 0, out_zp = 0;
  double in_scale = 1, out_scale = 1, w_scale = 1;

  if (auto filterOp = dyn_cast<top::WeightOp>(op.getRight().getDefiningOp())) {
    auto filter_f32 = filterOp.read<float>();
    int bitwidth = 4;
    Value value;
    if (op.getInInt4Scale().has_value()) {
      // 存在int4的输入scale，说明上一层是int8，故输入tensor也是int8，需要requant为int4
      in_scale =
          op->getAttr("in_int4_scale").cast<FloatAttr>().getValueAsDouble();
      in_zp = op->getAttr("in_int4_zp").cast<FloatAttr>().getValueAsDouble();
      module::getScaleAndZeroPoint(op.getInput(), in_int8_scale, in_int8_zp,
                                   asymmetric);
      // input int8, requant to int4
      //  auto ctx = op.getInput().getContext();
      //  auto cali_type = module::getCalibratedType(op.getInput());
      //  auto qtype =
      //  quant::UniformQuantizedType::get(quant::QuantizationFlags::Signed,
      //                                                IntegerType::get(ctx,
      //                                                4),
      //                                                cali_type.getExpressedType(),
      //                                                in_scale, in_zp, -8, 7);
      //  auto output_type =
      //  RankedTensorType::get(op.getInput().getType().cast<RankedTensorType>().getShape(),
      //  qtype);
      auto output_type = getQuantIntType(op.getInput(), in_scale, in_zp, 4);
      double scale = in_int8_scale / in_scale; // 将int8转为int4的rq参数
      double offset = in_zp - in_int8_zp * scale;
      auto to_name = "to_b4_for_" + module::getName(op.getOperation()).str();
      value = do_requantFp(op.getInput(), scale, offset, output_type, to_name);
      operands.push_back(value);
    } else { // 输入tensor也是int4
      operands.push_back(op.getInput());
      module::getScaleAndZeroPoint(op.getInput(), in_scale, in_zp, asymmetric,
                                   bitwidth);
    }
    module::getScaleAndZeroPoint(op.getOutput(), out_scale, out_zp, asymmetric,
                                 bitwidth);
    if (p.batch > 1 && in_zp != 0) { // Cannot merge zp to bias in BatchMatMul
      LoweringF32(rewriter, op);
      return;
    }
    f64_array_t weight_scale_v;
    if (filterOp.getScale().has_value()) {
      weight_scale_v = module::getF64Array(filterOp.getScale().value());
      w_scale = weight_scale_v->data()[0];
    } else {
      double w_max = findMaxabs(filter_f32->data(), filter_f32->size());
      w_scale = w_max / 7.0;
    }

    auto filter_int8 =
        std::make_shared<std::vector<int8_t>>(filter_f32->size());
    for (uint64_t t = 0; t < filter_f32->size(); t++) {
      filter_int8->at(t) = to_int8(filter_f32->at(t) / w_scale);
    }

    i32_array_t bias_int32;
    std::shared_ptr<std::vector<float>> bias_fp32;
    if (p.with_bias) {
      auto biasOp = cast<top::WeightOp>(op.getBias().getDefiningOp());
      bias_fp32 = biasOp.read<float>();
      bias_int32 = std::make_shared<std::vector<int32_t>>(bias_fp32->size());
    } else if (in_zp) {
      bias_int32 = std::make_shared<std::vector<int32_t>>(p.N);
    }

    for (int j = 0; j < p.N; j++) { // vector [1xN]
      int64_t bias_w_xz = 0;
      for (int i = 0; i < p.K; i++) {
        bias_w_xz += (int64_t)filter_int8->at(i * p.N + j) * in_zp;
      }

      if (p.with_bias) {
        bias_int32->data()[j] =
            std::round(bias_fp32->at(j) / (w_scale * in_scale) - bias_w_xz);
      } else if (in_zp) {
        bias_int32->data()[j] = -bias_w_xz;
      }
    }

    for (auto user : op->getUsers()) {
      if (isa<top::ConvOp>(user) || isa<top::MatMulOp>(user)) {
        all_next_layer_is_int8 = false;
      } else {
        all_next_layer_is_int4 = false;
      }

      if (isa<ReturnOp>(user)) {
        all_next_layer_is_int4 = true;
        all_next_layer_is_int8 = false;
        break;
      }
    }

    llvm::errs() << "all_next_layer_is_int4:" << all_next_layer_is_int4
                 << ",all_next_layer_is_int8:" << all_next_layer_is_int8
                 << "\n";
    if (all_next_layer_is_int8)
      llvm::errs() << "directly output int8\n";
    else
      llvm::errs() << "directly output int4\n";

    bool with_bias = p.with_bias || in_zp != 0;
    float scale_f;
    if (all_next_layer_is_int8) {
      if (op.getOutInt8Scale().has_value()) {
        out_int8_scale =
            op.getOutInt8Scale().value_or(APFloat(1.0)).convertToDouble();
      }
      scale_f = w_scale * in_scale / out_int8_scale;
    } else {
      scale_f = in_scale * w_scale / out_scale;
    }
    get_scale_and_shift(scale_f, scale, shift, 32);
    auto filter_type = op.getRight().getType().cast<RankedTensorType>();
    auto new_type =
        RankedTensorType::get(filter_type.getShape(), rewriter.getI8Type());
    auto new_filter =
        top::WeightOp::create(op, "filter_int8", *filter_int8, new_type);
    // operands.push_back(op.getInput());
    operands.push_back(new_filter);
    auto new_bias = op.getBias();
    if (with_bias) {
      std::vector<int64_t> shape = {p.N};
      auto new_type = RankedTensorType::get(shape, rewriter.getI32Type());
      new_bias = top::WeightOp::create(op, "bias_int32", *bias_int32, new_type);
      operands.push_back(new_bias);
    } else {
      auto none = module::getNoneOp(op);
      operands.push_back(none);
    }
  } else if (asymmetric) {
    LoweringF32(rewriter, op);
    return;
  } else { // mutable tensor or MatMul
    int64_t in_zp = 0, w_zp = 0, out_zp = 0;
    double in_scale = 1, w_scale = 1, out_scale = 1;
    module::getScaleAndZeroPoint(op.getInput(), in_scale, in_zp, asymmetric);
    module::getScaleAndZeroPoint(op.getRight(), w_scale, w_zp, asymmetric);
    module::getScaleAndZeroPoint(op.getOutput(), out_scale, out_zp, asymmetric);
    float scale_f = in_scale * w_scale / out_scale;
    get_scale_and_shift(scale_f, scale, shift, 32);
    for (auto operand : op.getOperands())
      operands.push_back(operand);
    if (p.with_bias) {
      auto biasOp = cast<top::WeightOp>(op.getBias().getDefiningOp());
      auto bias_fp32 = biasOp.read<float>();
      int bias_n = bias_fp32->size();
      auto bias_int32 = std::make_shared<std::vector<int32_t>>(bias_n);
      for (int j = 0; j < bias_n; j++) {
        bias_int32->data()[j] =
            std::round(bias_fp32->at(j) / (w_scale * in_scale));
      }
      auto new_type = RankedTensorType::get({bias_n}, rewriter.getI32Type());
      auto new_bias =
          top::WeightOp::create(op, "bias_int32", *bias_int32, new_type);
      operands[2] = new_bias;
    }
  }
  attrs.push_back(
      rewriter.getNamedAttr("rshifts", rewriter.getI64ArrayAttr(shift)));
  attrs.push_back(
      rewriter.getNamedAttr("multipliers", rewriter.getI64ArrayAttr(scale)));

  for (auto &attr : op->getAttrs()) {
    attrs.push_back(attr);
  }

  auto newType = getQuantInt4Type(op.getOutput(), asymmetric);
  if (all_next_layer_is_int8)
    newType = getQuantInt8Type(op.getOutput(), asymmetric);
  auto newOp =
      rewriter.create<tpu::MatMulOp>(op->getLoc(), newType, operands, attrs);
  rewriter.replaceOp(op, {newOp.getOutput()});

  if (!all_next_layer_is_int8 && !all_next_layer_is_int4) {
    bool first = true;
    Value value;
    for (auto user : op->getUsers()) {
      if (!isa<top::ConvOp>(user) && !isa<top::MatMulOp>(user)) {
        if (first) {
          first = false;
          if (op.getOutInt8Scale().has_value()) {
            out_int8_scale =
                op.getOutInt8Scale().value_or(APFloat(1.0)).convertToDouble();
            out_int8_zp =
                op.getOutInt8Zp().value_or(APFloat(0.0)).convertToDouble();
          }
          // requant to int8
          double scale = out_scale / out_int8_scale;
          double offset = out_int8_zp - out_zp * scale;
          auto output_type =
              getQuantIntType(op.getOutput(), out_int8_scale, out_int8_zp);
          auto to_name = module::getName(op.getOperation()).str() + "to_b8";
          value = do_requantFp(newOp.getOutput(), scale, offset, output_type,
                               to_name);
          llvm::errs() << "MatMul output requantFp, to_name:" << to_name
                       << ",value:";
          value.dump();
        }
        for (uint32_t idx = 0; idx < user->getNumOperands(); idx++) {
          if (op.getOutput() == user->getOperand(idx)) {
            llvm::errs() << "setOperand, idx:" << idx << "\n";
            user->setOperand(idx, value);
          }
        }
      }
    }
  }
}
void MatMulLowering::LoweringBF16(PatternRewriter &rewriter,
                                  top::MatMulOp op) const {
  lowering_common_bf16<tpu::MatMulOp>(rewriter, op);
}

void MatMulLowering::LoweringF16(PatternRewriter &rewriter,
                                 top::MatMulOp op) const {
  lowering_common_f16<tpu::MatMulOp>(rewriter, op);
}

void MatMulLowering::LoweringQuantized(PatternRewriter &rewriter,
                                       top::MatMulOp op) const {
  if (!module::isUniformQuantized(op.getInput(), op.getRight(),
                                  op.getOutput())) {
    llvm_unreachable("input output should be quantized");
  }
  auto p = op.parseParam();
  // assert(batch == 1);
  auto input_qtype = module::getUniformQuantizedType(op.getInput());
  auto right_qtype = module::getUniformQuantizedType(op.getRight());
  auto output_qtype = module::getUniformQuantizedType(op.getOutput());

  const double real_multiplier =
      input_qtype.getScale() * right_qtype.getScale() / output_qtype.getScale();
  int64_t multiplier, shift;
  QuantizeMultiplier(real_multiplier, &multiplier, &shift);
  int32_t right_zero_point = right_qtype.getZeroPoint();
  auto ctx = getContext();
  OpBuilder builder(ctx);
  std::vector<Value> operands;
  operands.push_back(op.getInput());
  if (isa<top::WeightOp>(op.getRight().getDefiningOp())) {
    auto right_stype = module::getStorageType(op.getRight());
    auto right_new_type =
        RankedTensorType::get(module::getShape(op.getRight()), right_stype);
    op.getRight().setType(right_new_type);
  }

  operands.push_back(op.getRight());
  if (p.with_bias) {
    auto bias_stype = module::getStorageType(op.getBias());
    auto bias_new_type =
        RankedTensorType::get(module::getShape(op.getBias()), bias_stype);
    op.getBias().setType(bias_new_type);
  }

  // std::string suffix = "_matmul";
  // std::string new_name = module::getName(op).str() + suffix;
  std::vector<NamedAttribute> attrs;
  for (auto &attr : op->getAttrs()) {
    attrs.push_back(attr);
  }
  if (right_zero_point)
    attrs.push_back(rewriter.getNamedAttr(
        "right_zp", rewriter.getI64IntegerAttr(right_zero_point)));

  int32_t input_zeroPoint = input_qtype.getZeroPoint();
  bool can_merge_izp =
      input_zeroPoint == 0 || isa<top::WeightOp>(op.getRight().getDefiningOp());
  auto right_type = op.getRight().getType().cast<RankedTensorType>();
  int K_idx = op.getRightTranspose() ? 1 : 0;
  int N_idx = op.getRightTranspose() ? 0 : 1;
  if (p.batch > 1) {
    K_idx++;
    N_idx++;
  }
  int64_t row_size = p.K;
  int64_t col_size = p.N;
  i32_array_t bias_quant;
  if (isa<top::WeightOp>(op.getBias().getDefiningOp())) {
    bias_quant =
        cast<top::WeightOp>(op.getBias().getDefiningOp()).read<int32_t>();
  } else {
    bias_quant = i32_array_t(new std::vector<int32_t>(col_size, 0));
  }
  auto bias_type = RankedTensorType::get({col_size}, rewriter.getI32Type());

  if (can_merge_izp) {
    //    attrs.push_back(rewriter.getNamedAttr(
    //        "multipliers", rewriter.getI64ArrayAttr(multiplier)));
    //    attrs.push_back(
    //        rewriter.getNamedAttr("rshifts",
    //        rewriter.getI64ArrayAttr(-shift)));
    //    attrs.push_back(rewriter.getNamedAttr(
    //        "quant_mode",
    //        tpu::RequantModeAttr::get(ctx, tpu::RequantMode::TFlite_Lshift)));
    if (input_zeroPoint) {
      // merge input_zeroPoint to bias
      std::shared_ptr<std::vector<int8_t>> right_quant;
      right_quant =
          cast<top::WeightOp>(op.getRight().getDefiningOp()).read<int8_t>();
      for (size_t r_ind = 0; r_ind < row_size; ++r_ind) {
        for (size_t c_ind = 0; c_ind < col_size; ++c_ind) {
          auto right_data = op.getRightTranspose()
                                ? right_quant->at(c_ind * row_size + r_ind)
                                : right_quant->at(c_ind + r_ind * col_size);
          bias_quant->data()[c_ind] -=
              input_zeroPoint * (right_data - right_zero_point);
        }
      }
      auto new_bias = top::WeightOp::create(op, "MergedInputZeroPoint",
                                            *bias_quant, bias_type);
      operands.push_back(new_bias);
    } else {
      operands.push_back(op.getBias());
    }
    auto matmul_type = RankedTensorType::get(module::getShape(op.getOutput()),
                                             rewriter.getI32Type());
    auto new_name = module::getName(op.getOperation()).str() + "_matmul_no_izp";
    auto name_loc = NameLoc::get(rewriter.getStringAttr(new_name));
    rewriter.setInsertionPointAfter(op);
    auto newOp =
        rewriter.create<tpu::MatMulOp>(name_loc, matmul_type, operands, attrs);
    // do requant
    auto newValue =
        do_requant(op->getLoc(), newOp.getOutput(), op.getOutput().getType(),
                   true, multiplier, shift, tpu::RequantMode::TFlite_Lshift);
    rewriter.replaceOp(op, {newValue});
    // rewriter.replaceOpWithNewOp<tpu::MatMulOp>(op, op.getOutput().getType(),
    //                                            operands, attrs);
  } else {
    // (M * K) (K * N)
    // (Input - izp) Matmul (Right - kzp) ==> (Input) Matmul (Right - kzp) -
    // (izp) Matmul (Right - kzp)
    // - (izp) Matmul (Right - kzp) ==> izp * kzp * K - izp *
    // reduce_sum(Right.col) for each row

    // merge izp * kzp * K to bias
    // for (size_t c_ind = 0; c_ind < col_size; ++c_ind) {
    //   bias_quant->data()[c_ind] +=
    //       input_zeroPoint * right_zero_point * row_size;
    // }
    // auto new_bias = top::WeightOp::create(op, "MergedInputZeroPoint",
    //                                       *bias_quant, bias_type);
    // operands.push_back(new_bias);
    operands.push_back(op.getBias());
    if (input_zeroPoint)
      attrs.push_back(rewriter.getNamedAttr(
          "input_zp", rewriter.getI64IntegerAttr(input_zeroPoint)));
    auto matmul_type = RankedTensorType::get(module::getShape(op.getOutput()),
                                             rewriter.getI32Type());
    auto new_name = module::getName(op.getOperation()).str() + "_int32";
    auto name_loc = NameLoc::get(rewriter.getStringAttr(new_name));
    rewriter.setInsertionPointAfter(op);
    auto newOp =
        rewriter.create<tpu::MatMulOp>(name_loc, matmul_type, operands, attrs);

    // rewriter.replaceOpWithNewOp<tpu::MatMulOp>(op, op.getOutput().getType(),
    //                                            operands, attrs);
#if 0
    // do reduce
    new_name = module::getName(op.getRight()).str() + "_reduce_h";
    attrs.erase(attrs.begin(), attrs.end());
    attrs.push_back(
        rewriter.getNamedAttr("axes", rewriter.getI64ArrayAttr({K_idx})));
    attrs.push_back(
        rewriter.getNamedAttr("keepdims", rewriter.getI64IntegerAttr(1)));
    attrs.push_back(
        rewriter.getNamedAttr("mode", rewriter.getStringAttr("ReduceSum")));
    auto reduce_shape = std::vector<int64_t>(module::getShape(op.getRight()));
    reduce_shape[K_idx] = 1;

    auto newType = RankedTensorType::get(reduce_shape, rewriter.getI32Type());
    if (op.getRightTranspose())
      newType = RankedTensorType::get(reduce_shape, rewriter.getI32Type());
    name_loc = NameLoc::get(rewriter.getStringAttr(new_name));
    rewriter.setInsertionPointAfterValue(op.getRight());
    auto reduceOp = rewriter.create<tpu::ReduceOp>(
        name_loc, newType,
        ValueRange{op.getRight(), module::getNoneOp(op), module::getNoneOp(op)},
        attrs);
    Value newValue = reduceOp.getOutput();
    // do reshape
    attrs.erase(attrs.begin(), attrs.end());
    if (op.getRightTranspose()) {
      auto reshapeType =
          RankedTensorType::get({1, col_size}, rewriter.getI32Type());
      newValue = do_reshape(newValue, reshapeType);
    }
    // do mulconst
    newValue = do_binary_saclar<tpu::MulConstOp>(
        newValue, rewriter.getI32Type(), -input_zeroPoint);
    // do add
    new_name = module::getName(newOp.getOutput()).str() + "_add_zp";
    name_loc = NameLoc::get(rewriter.getStringAttr(new_name));
    rewriter.setInsertionPointAfterValue(newOp);
    auto addOp = rewriter.create<tpu::AddOp>(
        name_loc, matmul_type, ValueRange{newOp.getOutput(), newValue}, attrs);
#endif
    // do requant
    auto newValue =
        do_requant(op->getLoc(), newOp.getOutput(), op.getOutput().getType(),
                   true, multiplier, shift, tpu::RequantMode::TFlite_Lshift);
    rewriter.replaceOp(op, {newValue});
  }
}

} // namespace bm1684x
} // namespace tpu_mlir
