//===----------------------------------------------------------------------===//
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "tpu_mlir/Dialect/Top/IR/TopOps.h"

#include "mlir/IR/Location.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "tpu_mlir/Support/Module.h"

using namespace llvm;
namespace tpu_mlir {

template <typename OpTy>
class TopLoweringToLinalg : public OpRewritePattern<OpTy> {
public:
  using OpRewritePattern<OpTy>::OpRewritePattern;

  LogicalResult matchAndRewrite(OpTy opTy,
                                PatternRewriter &rewriter) const override {
    Lowering(rewriter, opTy);
    return success();
  }

  virtual void Lowering(PatternRewriter &rewriter, OpTy opTy) const {
    UNREACHABLE_OP("Not Implemented", opTy);
  }
};

} // namespace tpu_mlir
