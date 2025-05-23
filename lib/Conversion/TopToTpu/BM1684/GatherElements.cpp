//===----------------------------------------------------------------------===//
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Conversion/TopToTpu/LoweringBM1684.h"

namespace tpu_mlir {
namespace bm1684 {

static void LoweringGatherElements(PatternRewriter &rewriter,
                                   top::GatherElementsOp op) {

  std::vector<NamedAttribute> attrs;
  std::vector<NamedAttribute> cpu_param;
  attrs.emplace_back(rewriter.getNamedAttr(
      "cpu_op_name", rewriter.getStringAttr("gatherelements_pt")));
  for (auto &attr : op->getAttrs()) {
    cpu_param.push_back(attr);
  }
  attrs.emplace_back(
      rewriter.getNamedAttr("param", rewriter.getDictionaryAttr(cpu_param)));
  std::vector<Value> operands;
  operands.push_back(op.getInput());

  auto indices_op = dyn_cast<top::WeightOp>(op.getIndices().getDefiningOp());
  if (indices_op) {
    // convert fp32 indices into int32
    auto indices_data = indices_op.read<float>();
    std::vector<int32_t> indices_int32_v(
        module::getNumElements(op.getIndices()));
    for (int i = 0; i < module::getNumElements(op.getIndices()); ++i) {
      indices_int32_v[i] = static_cast<int32_t>(indices_data->at(i));
    }
    auto new_type = RankedTensorType::get(module::getShape(op.getIndices()),
                                          rewriter.getI32Type());
    i32_array_t indices_int32 =
        std::make_shared<std::vector<int32_t>>(indices_int32_v);
    auto new_indices_op =
        top::WeightOp::create(op, "indices_int32", *indices_int32, new_type);
    operands.push_back(new_indices_op);
  } else {
    operands.push_back(op.getIndices());
  }
  rewriter.replaceOpWithNewOp<tpu::GenericCpuOp>(op, op.getOutput().getType(),
                                                 operands, attrs);
}

void GatherElementsLowering::LoweringF32(PatternRewriter &rewriter,
                                         top::GatherElementsOp op) const {
  LoweringGatherElements(rewriter, op);
}

void GatherElementsLowering::LoweringINT8(PatternRewriter &rewriter,
                                          top::GatherElementsOp op,
                                          bool asymmetric) const {
  LoweringF32(rewriter, op);
}
} // namespace bm1684
} // namespace tpu_mlir
