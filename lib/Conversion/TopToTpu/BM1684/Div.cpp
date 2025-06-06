//===----------------------------------------------------------------------===//
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Conversion/TopToTpu/LoweringBM1684.h"

namespace tpu_mlir {
namespace bm1684 {

void DivLowering::LoweringF32(PatternRewriter &rewriter, top::DivOp op) const {
  lowering_common_f32<tpu::DivOp>(rewriter, op);
}

void DivLowering::LoweringINT8(PatternRewriter &rewriter, top::DivOp op,
                               bool asymmetric) const {
  lowering_common_f32<tpu::DivOp>(rewriter, op);
}

} // namespace bm1684
} // namespace tpu_mlir
