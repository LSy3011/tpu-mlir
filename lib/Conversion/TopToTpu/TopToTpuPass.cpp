//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//
#include "tpu_mlir/Conversion/TopToTpu/LoweringBM1684.h"
#include "tpu_mlir/Conversion/TopToTpu/LoweringBM1684X.h"
#include "tpu_mlir/Conversion/TopToTpu/LoweringCV18xx.h"
#include "tpu_mlir/Conversion/TopToTpu/TopToTpu.h"
#include "tpu_mlir/Support/Module.h"
#include <fstream>
#include <regex>
#include <sstream>

namespace mlir {
#define GEN_PASS_DEF_CONVERTTOPTOTPU
#include "tpu_mlir/Conversion/Passes.h.inc"
} // namespace mlir

namespace tpu_mlir {

static void BackwardReshape(top::ReshapeOp op) {
  auto in = op.getInput();
  auto out = op.getOutput();
  auto in_type = in.getType().cast<RankedTensorType>();
  auto out_qtype = module::getCalibratedType(out);
  auto new_type = RankedTensorType::get(in_type.getShape(), out_qtype);
  in.setType(new_type);
  if (auto reshapeOp = dyn_cast<top::ReshapeOp>(in.getDefiningOp())) {
    BackwardReshape(reshapeOp);
  }
}

static void ForwardReshape(top::ReshapeOp op) {
  auto in = op.getInput();
  auto out = op.getOutput();
  auto out_type = out.getType().cast<RankedTensorType>();
  auto in_qtype = module::getCalibratedType(in);
  auto new_type = RankedTensorType::get(out_type.getShape(), in_qtype);
  out.setType(new_type);
  if (auto reshapeOp = dyn_cast<top::ReshapeOp>(in.getDefiningOp())) {
    ForwardReshape(reshapeOp);
  }
}

static void BackwardPermute(top::PermuteOp op) {
  auto in = op.getInput();
  auto out = op.getOutput();
  auto in_type = in.getType().cast<RankedTensorType>();
  auto out_qtype = module::getCalibratedType(out);
  auto new_type = RankedTensorType::get(in_type.getShape(), out_qtype);
  in.setType(new_type);
  if (auto permuteOp = dyn_cast<top::PermuteOp>(in.getDefiningOp())) {
    BackwardPermute(permuteOp);
  }
}

static void ForwardPermute(top::PermuteOp op) {
  auto in = op.getInput();
  auto out = op.getOutput();
  auto out_type = out.getType().cast<RankedTensorType>();
  auto in_qtype = module::getCalibratedType(in);
  auto new_type = RankedTensorType::get(out_type.getShape(), in_qtype);
  out.setType(new_type);
  if (auto permuteOp = dyn_cast<top::PermuteOp>(in.getDefiningOp())) {
    ForwardPermute(permuteOp);
  }
}

template <typename TyOp>
struct ForwardCalibartion : public OpRewritePattern<TyOp> {
  using OpRewritePattern<TyOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(TyOp op,
                                PatternRewriter &rewriter) const override {
    if constexpr (std::is_same_v<TyOp, top::ReduceOp>) {
      std::string mode = op.getMode().str();
      if (mode != "ReduceMax" && mode != "ReduceMin") {
        return failure();
      }
    }
    Value in = op.getInput();
    Value out = op.getOutput();
    if (!module::isCalibratedType(in)) {
      return failure();
    }
    auto in_qtype = module::getCalibratedType(in);
    if (module::isCalibratedType(out)) {
      auto out_qtype = module::getCalibratedType(out);
      if (in_qtype.getMax() == out_qtype.getMax() &&
          in_qtype.getMin() == out_qtype.getMin()) {
        return failure();
      }
    }
    auto out_type = out.getType().cast<RankedTensorType>();
    auto new_type = RankedTensorType::get(out_type.getShape(), in_qtype);
    out.setType(new_type);
    if (auto reshapeOp = dyn_cast<top::ReshapeOp>(out.getDefiningOp())) {
      ForwardReshape(reshapeOp);
    } else if (auto permuteOp = dyn_cast<top::PermuteOp>(out.getDefiningOp())) {
      ForwardPermute(permuteOp);
    }
    return success();
  }
};

template <typename TyOp, bool KeepMin = false>
struct BackwardCalibartion : public OpRewritePattern<TyOp> {
  using OpRewritePattern<TyOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(TyOp op,
                                PatternRewriter &rewriter) const override {
    Value in = op->getOperand(0);
    Value out = op.getOutput();
    if (!module::isCalibratedType(out)) {
      return failure();
    }
    if (in.hasOneUse() == false) {
      return failure();
    }
    auto in_qtype = module::getCalibratedType(in);
    auto out_qtype = module::getCalibratedType(out);
    if (module::isCalibratedType(in)) {
      auto in_qtype = module::getCalibratedType(in);
      if (in_qtype.getMax() == out_qtype.getMax() &&
          (KeepMin || in_qtype.getMin() == out_qtype.getMin())) {
        return failure();
      }
    }
    auto in_type = in.getType().cast<RankedTensorType>();
    if (KeepMin) {
      auto etype = module::getStorageType(out);
      out_qtype = quant::CalibratedQuantizedType::get(etype, in_qtype.getMin(),
                                                      out_qtype.getMax());
    }
    auto new_type = RankedTensorType::get(in_type.getShape(), out_qtype);
    in.setType(new_type);
    if (auto reshapeOp = dyn_cast<top::ReshapeOp>(in.getDefiningOp())) {
      BackwardReshape(reshapeOp);
    } else if (auto permuteOp = dyn_cast<top::PermuteOp>(in.getDefiningOp())) {
      BackwardPermute(permuteOp);
    }
    return success();
  }
};

template <typename TyOp>
struct ForwardTypePattern : public OpRewritePattern<TyOp> {
  using OpRewritePattern<TyOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(TyOp op,
                                PatternRewriter &rewriter) const override {
    Value in = op.getInput();
    Value out = op.getOutput();
    auto in_type = in.getType().cast<RankedTensorType>();
    auto out_type = out.getType().cast<RankedTensorType>();
    auto in_etype = in_type.getElementType();
    auto out_etype = out_type.getElementType();
    if (in_etype == out_etype) {
      return failure();
    }
    auto new_type = RankedTensorType::get(out_type.getShape(), in_etype);
    out.setType(new_type);
    return success();
  }
};

// to make compare inputs have the same min max
struct CompareCalibartion : public OpRewritePattern<top::CompareOp> {
  using OpRewritePattern<top::CompareOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(top::CompareOp op,
                                PatternRewriter &rewriter) const override {
    Value l = op.getLhs();
    Value r = op.getRhs();
    if (false == module::isCalibratedType(l) ||
        false == module::isCalibratedType(r)) {
      return failure();
    }
    auto stype = module::getStorageType(l);
    auto l_ctype = module::getCalibratedType(l);
    auto r_ctype = module::getCalibratedType(r);
    auto max = std::max(l_ctype.getMax(), r_ctype.getMax());
    auto min = std::min(l_ctype.getMin(), r_ctype.getMin());
    if (l_ctype.getMax() == r_ctype.getMax() &&
        l_ctype.getMin() == r_ctype.getMin()) {
      return failure();
    }
    auto new_ctype = quant::CalibratedQuantizedType::get(stype, min, max);
    auto new_ltype = RankedTensorType::get(module::getShape(l), new_ctype);
    auto new_rtype = RankedTensorType::get(module::getShape(r), new_ctype);
    l.setType(new_ltype);
    r.setType(new_rtype);
    return success();
  }
};

template <typename TyOp>
struct BackwardMutiInSingleOut : public OpRewritePattern<TyOp> {
  using OpRewritePattern<TyOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(TyOp op,
                                PatternRewriter &rewriter) const override {
    // TODO: need to be more clever
    for (auto in : op.getInputs()) {
      if (!module::isCalibratedType(in)) {
        return failure();
      }
      if (in.hasOneUse()) {
        continue;
      }
      for (auto user : in.getUsers()) {
        if (!isa<top::MaxPoolOp>(user) && user != op.getOperation()) {
          return failure();
        }
      }
    }

    Value out = op.getOutput();
    if (!module::isCalibratedType(out)) {
      return failure();
    }
    auto out_qtype = module::getCalibratedType(out);
    // checkout all input cali is the same
    auto in_0 = op.getInputs()[0];
    auto in_0_qtype = module::getCalibratedType(in_0);
    bool same = true;
    for (uint i = 1; i < op.getInputs().size(); i++) {
      auto qtype = module::getCalibratedType(op.getInputs()[i]);
      if (qtype.getMin() != in_0_qtype.getMin() ||
          qtype.getMax() != in_0_qtype.getMax()) {
        same = false;
        break;
      }
    }
    if (same) {
      if (out_qtype.getMin() == in_0_qtype.getMin() &&
          out_qtype.getMax() == in_0_qtype.getMax()) {
        // do nothing
        return failure();
      }
      auto out_type = out.getType().cast<RankedTensorType>();
      auto new_type = RankedTensorType::get(out_type.getShape(), in_0_qtype);
      out.setType(new_type);
      return success();
    }

    for (Value in : op.getInputs()) {
      auto in_type = in.getType().cast<RankedTensorType>();
      auto new_type = RankedTensorType::get(in_type.getShape(), out_qtype);
      in.setType(new_type);
      if (auto reshapeOp = dyn_cast<top::ReshapeOp>(in.getDefiningOp())) {
        BackwardReshape(reshapeOp);
      } else if (auto permuteOp =
                     dyn_cast<top::PermuteOp>(in.getDefiningOp())) {
        BackwardPermute(permuteOp);
      }
    }
    return success();
  }
};

struct ConvertTopToTpu : public ::impl::ConvertTopToTpuBase<ConvertTopToTpu> {
public:
  void runOnOperation() override {
    module_ = getOperation();
    ctx_ = &getContext();
    mainFunc_ = module::getMainFuncOp();
    LoweringConfig::isQuantized = false;
    module::setChip(StringRef(chip).upper());
    module::setMode(StringRef(mode).upper());

    if (module::isState(module::State::TOP_QUANTIZED)) {
      module::setAsymmetric(true);
      LoweringConfig::isQuantized = true;
    } else {
      LoweringConfig::isQuantized = false;
      module::setAsymmetric(isAsymmetric);
      if (module::isCV18xx()) {
        all_int8_process();
        module::updateModuleTypes();
      }
      calibration_process();
    }
    init_qtable();
    RewritePatternSet patterns(ctx_);
    ConversionTarget target(*ctx_);
    target.addLegalDialect<tpu::TpuDialect, func::FuncDialect>();
    // no need to lowering:
    target.addLegalOp<top::InputOp, top::WeightOp, top::NoneOp>();
    if (module::isBM1684XFamily()) {
      bm1684x::populateTopToTpuConversionPatterns(&patterns);
    } else if (module::isBM1684Family()) {
      bm1684::populateTopToTpuConversionPatterns(&patterns);
    } else if (module::isCV18xx()) {
      cv18xx::populateTopToTpuConversionPatterns(&patterns);
    } else {
      llvm_unreachable("Not Implemented");
    }
    auto config = GreedyRewriteConfig();
    config.maxIterations = 0; // apply each pattern only once.
    applyPatternsAndFoldGreedily(module_, std::move(patterns), config);
    // if (failed(
    //         applyPartialConversion(module_, target, std::move(patterns)))) {
    //   signalPassFailure();
    // }
    // adjust reshape
    patterns.clear();
    patterns.add<ForwardTypePattern<tpu::ReshapeOp>>(ctx_);
    applyPatternsAndFoldGreedily(module_, std::move(patterns));
    cast_process();
    relu_process();
    module::updateModuleTypes();
    module::setState(module::State::TPU_LOWERED);
  }

protected:
  void calibration_process() {
    if (!module::isState(module::State::TOP_CALIBRATED)) {
      return;
    }
    // clang-format off
    RewritePatternSet patterns(ctx_);
    patterns.add<ForwardCalibartion<top::ReshapeOp>,
                 ForwardCalibartion<top::PermuteOp>>(ctx_);
    applyPatternsAndFoldGreedily(module_, std::move(patterns));
    patterns.clear();
    patterns.add<BackwardMutiInSingleOut<top::ConcatOp>,
                 BackwardMutiInSingleOut<top::MinOp>,
                 BackwardMutiInSingleOut<top::MaxOp>>(ctx_);
    applyPatternsAndFoldGreedily(module_, std::move(patterns));
    patterns.clear();
    patterns.add<BackwardCalibartion<top::ReluOp>,
                 BackwardCalibartion<top::MaxPoolOp>,
                 BackwardCalibartion<top::MaxPoolWithMaskOp>,
                 BackwardCalibartion<top::LeakyReluOp, true>,
                //  BackwardCalibartion<top::PReluOp>,
                 BackwardCalibartion<top::AbsOp>>(ctx_);
    if (!module::isCV18xx()) {
      // notice when it's dominated by negative value
      // and factor is very small it'll cause cumulative error
      patterns.add<BackwardCalibartion<top::PReluOp, true>>(ctx_);
    }
    applyPatternsAndFoldGreedily(module_, std::move(patterns));
    patterns.clear();
    patterns.add<CompareCalibartion>(ctx_);
    applyPatternsAndFoldGreedily(module_, std::move(patterns));
    patterns.clear();
    patterns.add<ForwardCalibartion<top::ReluOp>,
                 ForwardCalibartion<top::MaxPoolOp>,
                 ForwardCalibartion<top::MaxPoolWithMaskOp>,
                 ForwardCalibartion<top::MaxUnpoolOp>,
                 ForwardCalibartion<top::ReshapeOp>,
                 ForwardCalibartion<top::SliceOp>,
                 ForwardCalibartion<top::TileOp>,
                 ForwardCalibartion<top::PadOp>,
                 ForwardCalibartion<top::PermuteOp>,
                 ForwardCalibartion<top::ReverseOp>,
                 ForwardCalibartion<top::UpsampleOp>,
                 ForwardCalibartion<top::LeakyReluOp>,
                //  ForwardCalibartion<top::PReluOp>,
                 ForwardCalibartion<top::AbsOp>
                >(ctx_);
    // clang-format on
    if (!module::isCV18xx()) {
      // notice it will cause cumulative error
      patterns.add<ForwardCalibartion<top::PReluOp>>(ctx_);
    } else {
      patterns.add<ForwardCalibartion<top::ReduceOp>>(ctx_);
    }
    if (module::isBM1684Family()) {
      // TODO: support asymmetric mode
      patterns.add<ForwardCalibartion<top::AvgPoolOp>>(ctx_);
    }
    applyPatternsAndFoldGreedily(module_, std::move(patterns));
  }

  void all_int8_process() {
    auto retTypes = mainFunc_.getResultTypes();
    mainFunc_.walk([&](Operation *op) {
      if (isa<tpu_mlir::InferenceInterface>(op) || isa<top::InputOp>(op)) {
        for (auto value : op->getResults()) {
          if (value.getType().isa<mlir::NoneType>() ||
              !module::isCalibratedType(value)) {
            continue;
          }
          auto out_qtype = module::getCalibratedType(value);
          if (out_qtype.getMin() != -out_qtype.getMax()) {
            auto max = out_qtype.getMax();
            auto quant_type = quant::CalibratedQuantizedType::get(
                out_qtype.getExpressedType(), -max, max);
            auto new_type =
                RankedTensorType::get(module::getShape(value), quant_type);
            value.setType(new_type);
          }
        }
      }
    });
  }

  void relu_process() {
    Builder builder(ctx_);
    mainFunc_.walk([&](Operation *op) {
      if (module::isTpuOp(op)) {
        if (op->hasTrait<trait::SupportFuseRelu>() || isa<tpu::ReluOp>(op)) {
          if (module::isUniformQuantized(op->getResult(0)) ||
              module::isUniformQuantized(op->getOperand(0))) {
            op->setAttr("relu_limit", builder.getF64FloatAttr(-1.0));
          }
        }
      }
    });
  }

  void cast_process() {
    // return types
    auto retTypes = mainFunc_.getResultTypes();
    mainFunc_.walk([&](Operation *op) {
      bool is_tpu = module::isTpuOp(op);
      if (is_tpu || isa<ReturnOp>(op)) {
        for (uint32_t idx = 0; idx < op->getNumOperands(); idx++) {
          auto opd = op->getOperand(idx);
          TypeCastMode mode = TypeCastMode::DO_NOTHING;
          mlir::Type target_type;
          if (auto typeIf = dyn_cast<TypeInterface>(op)) {
            target_type = typeIf.type_verify(idx, mode);
          } else if (isa<ReturnOp>(op)) {
            // return op
            target_type = type_verify_case_type(op, idx, retTypes[idx], mode);
          } else {
            target_type = type_verify_case_same(op, idx, mode);
          }
          if (mode != TypeCastMode::DO_NOTHING) {
            auto castOp = do_cast(opd, target_type, mode);
            op->setOperand(idx, castOp);
          }
        }
      }
    });
  }

  Value do_cast(Value v, Type to, TypeCastMode mode) {
    auto from_stype = module::getStorageType(v);
    auto to_stype = module::getStorageType(to);
    // check whether value has been casted
    for (auto user : v.getUsers()) {
      if (false == isa<tpu::CastOp>(user) &&
          (false == isa<tpu::GenericCpuOp>(user) ||
           dyn_cast<tpu::GenericCpuOp>(user).getCpuOpName() != "quant")) {
        continue;
      }
      if (type_need_cast(user->getResult(0).getType(), to) == false) {
        return user->getResult(0);
      }
    }

    bool all_next_layer_is_int4 = false;
    if (module::getMode() == module::Mode::INT4) {
      all_next_layer_is_int4 = true;
      for (auto user : v.getUsers()) {
        if (!isa<tpu::Conv2DOp, tpu::MatMulOp>(user)) {
          all_next_layer_is_int4 = false;
        }
      }
    }

    auto ctx = v.getContext();
    OpBuilder builder(ctx);
    builder.setInsertionPointAfterValue(v);
    auto name = module::getName(v).str();
    switch (mode) {
    case TypeCastMode::DO_DEQUANTIZE:
    case TypeCastMode::DO_CAST: {
      name += "_" + type_string(to_stype);
      auto newType = RankedTensorType::get(module::getShape(v), to_stype);
      auto loc = NameLoc::get(builder.getStringAttr(name));
      auto castOp = builder.create<tpu::CastOp>(loc, newType, ValueRange{v});
      return castOp.getOutput();
    }
    case TypeCastMode::DO_QUANTIZE: {
      if (module::isCalibratedType(v) == false) {
        v.dump();
        llvm_unreachable("Only calibrated type can do quantize");
      }
      auto newType = getQuantInt8Type(v, module::isAsymmetric());
      if (all_next_layer_is_int4) {
        newType = getQuantInt4Type(v, module::isAsymmetric());
      }
      name += "_" + type_string(newType);
      auto loc = NameLoc::get(builder.getStringAttr(name));
      if (module::isCV18xx()) {
        auto parentOp = v.getDefiningOp();
        if (isa<top::InputOp>(parentOp)) {
          return insert_18xx_cpu_cast(builder, v, loc, newType);
        }
      }
      auto castOp = builder.create<tpu::CastOp>(loc, newType, ValueRange{v});
      return castOp.getOutput();
    }
    default:
      break;
    }
    return v;
  }

  Value insert_18xx_cpu_cast(OpBuilder &builder, Value &v, NameLoc &loc,
                             Type &newType) {
    float scale = module::getUniformQuantizedType(newType).getScale();
    scale = 1 / scale;
    std::vector<NamedAttribute> attrs;
    std::vector<NamedAttribute> param;
    attrs.emplace_back(
        builder.getNamedAttr("cpu_op_name", builder.getStringAttr("quant")));
    param.emplace_back(
        builder.getNamedAttr("from", builder.getStringAttr("FP32")));
    param.emplace_back(
        builder.getNamedAttr("to", builder.getStringAttr("INT8")));
    param.emplace_back(
        builder.getNamedAttr("scale", builder.getF32FloatAttr(scale)));
    attrs.emplace_back(
        builder.getNamedAttr("param", builder.getDictionaryAttr(param)));
    auto castOp = builder.create<tpu::GenericCpuOp>(
        loc, newType, ValueRange{v}, ArrayRef<NamedAttribute>{attrs});
    return castOp.getOutput();
  }

  static StringRef qmode(const std::string &mode) {
    std::string tmp = StringRef(mode).upper();
    if (tmp == module::Mode::INT8) {
      return module::Mode::INT8;
    }
    if (tmp == module::Mode::F16) {
      return module::Mode::F16;
    }
    if (tmp == module::Mode::BF16) {
      return module::Mode::BF16;
    }
    if (tmp == module::Mode::F32) {
      return module::Mode::F32;
    }
    llvm::errs() << "Unknown quantize mode: [" << mode << "]\n";
    llvm_unreachable("Unknown quantize mode");
    return "";
  }

  void init_qtable() {
    LoweringConfig::quantize_map.clear();
    if (qtable.empty()) {
      return;
    }
    std::regex map_pattern("\\S+\\s+\\S+");
    std::regex name_pattern("\\S+");
    std::regex info_pattern("#.*");
    std::regex empty_pattern("^\\s*$");
    std::ifstream infile(qtable);
    if (!infile) {
      llvm::errs() << "Can't open file: " << qtable << " !\n";
      llvm_unreachable("Open quantize table failed");
    }
    std::string line;
    while (std::getline(infile, line)) {
      if (line.back() == '\r') {
        line.pop_back();
      }
      std::istringstream iss(line);
      std::string name;
      std::string mode;
      if (std::regex_match(line, empty_pattern)) {
        continue;
      }
      if (std::regex_match(line, info_pattern)) {
        continue;
      }
      if (std::regex_match(line, map_pattern)) {
        iss >> name;
        iss >> mode;
        LoweringConfig::quantize_map[name] = qmode(mode);
        continue;
      }
      if (std::regex_match(line, name_pattern)) {
        continue;
      }

      llvm::errs() << "Error, quantize file in [" << line << "]\n";
      assert(false);
    }
  }

protected:
  ModuleOp module_;
  FuncOp mainFunc_;
  MLIRContext *ctx_;
};

std::unique_ptr<Pass> createConvertTopToTpu() {
  return std::make_unique<ConvertTopToTpu>();
}

} // namespace tpu_mlir
