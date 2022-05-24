#include "sophgo/Dialect/Tpu/IR/TpuOps.h"
#include "sophgo/Support/Dnnl/Dnnl.h"
#include "sophgo/Support/Helper/Quant.h"
#include "sophgo/Support/Helper/Module.h"

using namespace sophgo;
using namespace sophgo::helper;
using namespace mlir;

void tpu::AvgPoolOp::parseParam(int64_t &n, int64_t &c, int64_t &ih,
                                int64_t &iw, int64_t &oh, int64_t &ow,
                                int64_t &kh, int64_t &kw, int64_t &sh,
                                int64_t &sw, int64_t &pt, int64_t &pb,
                                int64_t &pl, int64_t &pr, int64_t &pad_value,
                                bool &relu, bool &is_global,
                                bool &count_include_pad) {
  auto i_s = input().getType().cast<RankedTensorType>().getShape();
  auto o_s = output().getType().cast<RankedTensorType>().getShape();
  kh = kernel_shape().getValue()[0].cast<IntegerAttr>().getInt();
  kw = kernel_shape().getValue()[1].cast<IntegerAttr>().getInt();
  sh = strides().getValue()[0].cast<IntegerAttr>().getInt();
  sw = strides().getValue()[1].cast<IntegerAttr>().getInt();
  relu = do_relu();
  size_t num_dims = i_s.size();
  assert(num_dims == 4); // 4 dims now
  n = i_s[0];
  c = i_s[1];
  ih = i_s[2];
  iw = i_s[3];
  oh = o_s[2];
  ow = o_s[3];
  pt = pads().getValue()[0].cast<IntegerAttr>().getInt();
  pl = pads().getValue()[1].cast<IntegerAttr>().getInt();
  pb = pads().getValue()[2].cast<IntegerAttr>().getInt();
  pr = pads().getValue()[3].cast<IntegerAttr>().getInt();
  is_global = false;
  if (kh == ih && kw == iw && oh == 1 && ow == 1) {
    is_global = true;
  }
  pad_value = this->pad_value();
  count_include_pad = this->count_include_pad();
}

LogicalResult tpu::AvgPoolOp::init(InferenceParameter &p) {
  auto pooling = new Pooling();
  int64_t n, c, ih, iw, oh, ow, kh, kw, sh, sw, pt, pb, pl, pr, pad_value;
  bool relu, is_global, count_include_pad;
  auto dt = getDnnlType(input());
  parseParam(n, c, ih, iw, oh, ow, kh, kw, sh, sw, pt, pb, pl, pr, pad_value,
             relu, is_global, count_include_pad);

  int izp = 0;
  auto dtype = input().getType().cast<RankedTensorType>().getElementType();
  if (dtype.isa<quant::UniformQuantizedType>()) {
    izp = dtype.cast<quant::UniformQuantizedType>().getZeroPoint();
  }

  pooling->setup(p.inputs[0], p.outputs[0], n, c, ih, iw, oh, ow, kh, kw, sh,
                 sw, pt, pb, pl, pr, true, count_include_pad, izp, pad_value,
                 dt);
  p.handle = (void *)pooling;
  return success();
}

void tpu::AvgPoolOp::deinit(InferenceParameter &p) {
  if (p.handle != nullptr) {
    auto pooling = (Pooling *)p.handle;
    delete pooling;
    p.handle = nullptr;
  }
  return;
}

LogicalResult tpu::AvgPoolOp::inference(InferenceParameter &p) {
  if (p.handle == nullptr) {
    return failure();
  }
  auto pooling = (Pooling *)p.handle;
  pooling->run();

  if (Quant::isUniformQuantized(input())) {
    auto i_qtype = Quant::getUniformQuantizedType(input());
    auto o_qtype = Quant::getUniformQuantizedType(output());
    auto num_elem = Module::getNumElements(output());
    double scale = i_qtype.getScale() / o_qtype.getScale();
#pragma omp parallel for schedule(static, omp_schedule(num_elem))
    for (int64_t i = 0; i < num_elem; ++i) {
      p.outputs[0][i] = (p.outputs[0][i] - i_qtype.getZeroPoint()) * scale;
    }
  }

  if (do_relu()) {
    relu(p.outputs[0], p.outputs[0], Module::getNumElements(output()));
  }

  if (Quant::isUniformQuantized(input())) {
    int64_t num_elem = Module::getNumElements(output());
    auto o_qtype = Quant::getUniformQuantizedType(output());
#pragma omp parallel for schedule(static, omp_schedule(num_elem))
    for (int64_t i = 0; i < num_elem; ++i) {
      p.outputs[0][i] =
          Quant::to_int8(p.outputs[0][i] + o_qtype.getZeroPoint());
    }
  }

#ifdef DEBUG_TPU_INFER
  llvm::errs() << "AvgPoolOp inference:" << this->name() << "\n";
  for (int i = 0; i < 5; i++) {
    printf("%d  %f -> %f\n", i, p.inputs[0][i], p.outputs[0][i]);
  }
#endif

  return success();
}

LogicalResult tpu::AvgPoolOp::LocalGenSupport() {
  int64_t n, c, ih, iw, oh, ow, kh, kw, sh, sw, pt, pb, pl, pr, pad_value;
  bool relu, is_global, count_include_pad;
  parseParam(n, c, ih, iw, oh, ow, kh, kw, sh, sw, pt, pb, pl, pr, pad_value,
             relu, is_global, count_include_pad);
  if (is_global == false && (sh > 15 || sw > 15)) {
    return failure();
  }
  return success();
}

LogicalResult tpu::AvgPoolOp::BackwardH(int64_t &in_idx, int64_t &in_slice,
                                        int64_t out_idx, int64_t out_slice) {
  int64_t n, c, ih, iw, oh, ow, kh, kw, sh, sw, pt, pb, pl, pr, pad_value;
  bool relu, is_global, count_include_pad;
  parseParam(n, c, ih, iw, oh, ow, kh, kw, sh, sw, pt, pb, pl, pr, pad_value,
             relu, is_global, count_include_pad);
  in_slice = (out_slice - 1) * sh + kh;
  in_idx = out_idx * sh - pt;
  LocalGenInterface::fixSlice(in_idx, in_slice, ih);
  return success();
}
