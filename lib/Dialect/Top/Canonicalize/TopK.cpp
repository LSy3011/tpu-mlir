//===----------------------------------------------------------------------===//
//

//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//
#include "tpu_mlir/Support/Module.h"
#include "tpu_mlir/Support/OpRewriterPatternEx.h"

using namespace tpu_mlir::top;

// SLice after TopK may lead to some address error
// Slice erasing will begin under the 2 condition:
// 1. Values and Indices only have one SliceOp user, and the parameters of the
// two slices equal
// 2. Only one SliceOp user for both outputs
struct TopKWithSlice : public OpRewriterPatternEx<TopKOp> {
  using OpRewriterPatternEx::OpRewriterPatternEx;

  TopKWithSlice(mlir::MLIRContext *context)
      : OpRewriterPatternEx<TopKOp>(context, "TopKWithSlice") {}
  LogicalResult matchAndRewriteImpl(TopKOp op,
                                    PatternRewriter &rewriter) const override {
    if (module::isDynamic())
      return failure();
    if (op.getIndices().hasOneUse() && op.getValues().hasOneUse()) {
      return compare_slice(op, rewriter);
    } else if (op.getIndices().hasOneUse() && op.getValues().use_empty()) {
      if (auto slice_indices_op =
              dyn_cast_or_null<SliceOp>(*op.getIndices().getUsers().begin())) {
        return which_axes(op, slice_indices_op, rewriter);
      } else {
        return failure();
      }
    } else if (op.getValues().hasOneUse() && op.getIndices().use_empty()) {
      if (auto slice_values_op =
              dyn_cast_or_null<SliceOp>(*op.getValues().getUsers().begin())) {
        return which_axes(op, slice_values_op, rewriter);
      } else {
        return failure();
      }
    } else {
      return failure();
    }
  }

private:
  LogicalResult slice2k(TopKOp op, SliceOp slice_op, PatternRewriter &rewriter,
                        int64_t axis) const {
    auto slice_shape = module::getShape(slice_op);
    auto offset = module::getI64Array(slice_op.getOffset());
    auto steps = module::getI64Array(slice_op.getSteps());
    auto ends = module::getI64Array(slice_op.getEnds());

    auto topk_shape = module::getShape(slice_op);

    if (axis != op.getAxis()) {
      return failure();
    }
    if (1 != steps->at(axis)) {
      return failure();
    }
    // slice the end of the array
    if (offset->at(axis) != 0 && ends->at(axis) >= topk_shape[axis]) {
      auto topk_in_shape = module::getShape(op.getInput());
      // we can slice the end of the array, only when topk output all data
      if (topk_in_shape[axis] != op.getK()) {
        return failure();
      }
      bool largest = op.getLargest();
      op->setAttr("largest", rewriter.getBoolAttr(!largest));
      // If slice the middle of  the array, fail
    } else if (offset->at(axis) != 0 && ends->at(axis) < topk_shape[axis]) {
      return failure();
    }

    auto ndims = topk_shape.size();
    std::vector<int64_t> new_shape(ndims, 1);
    for (int64_t i = 0; i < ndims; i++) {
      new_shape[i] = topk_shape[i];
    }
    new_shape[axis] = slice_shape[axis];

    module::setShape(op.getIndices(), new_shape);
    module::setShape(op.getValues(), new_shape);

    int64_t K = slice_shape[axis];

    op->setAttr("K", rewriter.getI64IntegerAttr(K));

    std::vector<Location> locs = {};
    std::string indices_name =
        module::getName(op.getIndices()).str() + "_Slice";
    std::string values_name = module::getName(op.getValues()).str() + "_Slice";

    auto indices_loc = NameLoc::get(rewriter.getStringAttr(indices_name));
    auto values_loc = NameLoc::get(rewriter.getStringAttr(values_name));
    locs.push_back(indices_loc);
    locs.push_back(values_loc);

    auto fused_loc = FusedLoc::get(getContext(), locs);
    op->setLoc(fused_loc);

    if (offset->at(axis) == 0 && ends->at(axis) < topk_shape[axis]) {
      return success();
      // reverse the order if slice the end of the array
    } else if (offset->at(axis) != 0 && ends->at(axis) >= topk_shape[axis]) {
      std::vector<NamedAttribute> attrs;
      attrs.push_back(
          rewriter.getNamedAttr("axis", rewriter.getI64IntegerAttr(axis)));

      auto slice_indices_op =
          dyn_cast_or_null<SliceOp>(*op.getIndices().getUsers().begin());
      auto slice_values_op =
          dyn_cast_or_null<SliceOp>(*op.getValues().getUsers().begin());
      rewriter.setInsertionPointAfter(op);
      if (slice_indices_op && slice_values_op) {
        rewriter.replaceOpWithNewOp<ReverseOp>(
            slice_indices_op, slice_indices_op.getResult().getType(),
            slice_indices_op.getInput(), attrs);
        rewriter.replaceOpWithNewOp<ReverseOp>(
            slice_values_op, slice_values_op.getResult().getType(),
            slice_values_op.getInput(), attrs);

      } else {
        rewriter.replaceOpWithNewOp<ReverseOp>(slice_op,
                                               slice_op.getResult().getType(),
                                               slice_op.getInput(), attrs);
      }
      return success();
    } else {
      return failure();
    }
  }

  LogicalResult which_axes(TopKOp op, SliceOp slice_op,
                           PatternRewriter &rewriter) const {
    if (!slice_op.getHasparamConvertAxesAttr().empty()) {
      auto axes = module::getI64Array(slice_op.getHasparamConvertAxesAttr());

      if (1 != axes->size()) {
        return failure();
      } else {
        auto axis = axes->at(0);
        return slice2k(op, slice_op, rewriter, axis);
      }
    } else {
      auto axes = module::getI64Array(slice_op.getAxes());
      if (1 != axes->size()) {
        return failure();
      } else {
        auto axis = axes->at(0);
        return slice2k(op, slice_op, rewriter, axis);
      }
    }
  }

  LogicalResult compare_slice(TopKOp op, PatternRewriter &rewriter) const {
    auto slice_indices_op =
        dyn_cast_or_null<SliceOp>(*op.getIndices().getUsers().begin());
    auto slice_values_op =
        dyn_cast_or_null<SliceOp>(*op.getValues().getUsers().begin());

    if (slice_indices_op || slice_values_op) {
      // two slice
      if (slice_indices_op && slice_values_op) {
        // two slices equal exactly
        if ((module::getShape(slice_indices_op) ==
             module::getShape(slice_values_op)) &
            (slice_indices_op.getHasparamConvertAxesAttr() ==
             slice_values_op.getHasparamConvertAxesAttr()) &
            (slice_indices_op.getAxes() == slice_values_op.getAxes()) &
            (slice_indices_op.getOffset() == slice_values_op.getOffset()) &
            (slice_indices_op.getSteps() == slice_values_op.getSteps()) &
            (slice_indices_op.getEnds() == slice_values_op.getEnds()) &
            (slice_indices_op.getOffsetT() == slice_values_op.getOffsetT()) &
            (slice_indices_op.getStepsT() == slice_values_op.getStepsT()) &
            (slice_indices_op.getEndsT() == slice_values_op.getEndsT())) {
          return which_axes(op, slice_indices_op, rewriter);
        } else {
          return failure();
        }
        // once slice
      } else {
        if (slice_indices_op) {
          return which_axes(op, slice_indices_op, rewriter);
        } else {
          return which_axes(op, slice_values_op, rewriter);
        }
      }
      // no slice
    } else {
      return failure();
    }
    return failure();
  }
};

void TopKOp::getCanonicalizationPatterns(RewritePatternSet &results,
                                         MLIRContext *context) {
  results.insert<TopKWithSlice>(context);
}
