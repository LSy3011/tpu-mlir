//===----------------------------------------------------------------------===//
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Conversion/TopToTpu/LoweringCV18xx.h"

#define DEBUG_TYPE "lowering-add"

namespace tpu_mlir {
namespace cv18xx {
void ReverseLowering::LoweringINT8(PatternRewriter &rewriter, top::ReverseOp op,
                                   bool asymmetric) const {
  lowering_common_int8<tpu::ReverseOp>(rewriter, op, asymmetric);
}

void ReverseLowering::LoweringBF16(PatternRewriter &rewriter,
                                   top::ReverseOp op) const {
  lowering_common_bf16<tpu::ReverseOp>(rewriter, op);
}

} // namespace cv18xx
} // namespace tpu_mlir
