//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Dialect/Tpu/IR/TpuOps.h"
#include "tpu_mlir/Support/Dnnl/Dnnl.h"

#include "tpu_mlir/Support/Module.h"
#include "tpu_mlir/Support/MathUtils.h"
#include "tpu_mlir/Support/Float16.h"
#include "float.h"



pool_attr_t tpu::MaxPoolWithMaskOp::parseParam() {
  pool_attr_t p = {0};
  p.id = 1;
  p.od = 1;
  p.kd = 1;
  p.sd = 1;
  auto ishape = getInput().getType().dyn_cast<RankedTensorType>().getShape();
  auto oshape = getOutput().getType().dyn_cast<RankedTensorType>().getShape();
  module::getNCHW(ishape, p.n, p.c, p.ih, p.iw);
  module::getNCHW(oshape, p.n, p.c, p.oh, p.ow);

  auto kernel = module::getI64Array(getKernelShape());
  p.kh = kernel->at(0);
  p.kw = kernel->at(1);
  auto stride = module::getI64Array(getStrides());
  p.sh = stride->at(0);
  p.sw = stride->at(1);
  auto pad = module::getI64Array(getPads());
  p.pad_h = pad->at(0);
  p.pad_w = pad->at(1);
  p.pad_h_after = pad->at(2);
  p.pad_w_after = pad->at(3);
  p.pad_value = 0;
  p.do_relu = getDoRelu();
  p.relu_limit = getReluLimit().convertToDouble();
  p.is_global = p.ih == p.kh && p.iw == p.kw && p.oh == 1 && p.ow == 1;
  p.count_include_pad = 0;
  return p;
}

LogicalResult tpu::MaxPoolWithMaskOp::init(InferenceParameter &p) {
  return success();
}

void tpu::MaxPoolWithMaskOp::deinit(InferenceParameter &p) { return; }

LogicalResult tpu::MaxPoolWithMaskOp::inference(InferenceParameter &p) {
  auto attr = parseParam();
  int64_t nc = attr.n * attr.c;
  auto num_elem = module::getNumElements(getOutput());
  std::fill_n(p.outputs[0], num_elem, (float)(-FLT_MAX));
#pragma omp parallel for schedule(static, omp_schedule(nc))
  for (int64_t idx = 0; idx < nc; ++idx) {
    auto bottom_data = p.inputs[0] + idx * attr.ih * attr.iw;
    auto top_data = p.outputs[0] + idx * attr.oh * attr.ow;
    auto top_mask = p.outputs[1] + idx * attr.oh * attr.ow;
    for (int64_t ph = 0; ph < attr.oh; ++ph) {
      for (int64_t pw = 0; pw < attr.ow; ++pw) {
        auto hstart = ph * attr.sh - attr.pad_h;
        auto wstart = pw * attr.sw - attr.pad_w;
        auto hend = std::min(hstart + attr.kh, attr.ih);
        auto wend = std::min(wstart + attr.kw, attr.iw);
        if (hstart < 0) {
          hstart = 0;
        }
        if (wstart < 0) {
          wstart = 0;
        }
        auto pool_index = ph * attr.ow + pw;
        for (int64_t h = hstart; h < hend; ++h) {
          for (int64_t w = wstart; w < wend; ++w) {
            auto index = h * attr.iw + w;
            if (bottom_data[index] > top_data[pool_index]) {
              top_data[pool_index] = bottom_data[index];
              top_mask[pool_index] = index;
            }
          }
        }
      }
    }
  }

  return success();
}

LogicalResult tpu::MaxPoolWithMaskOp::LocalGenSupport() {
  // auto stride = module::getI64Array(getStrides());
  // if ((stride->at(0) > 15 || stride->at(1) > 15)) {
  //   return failure();
  // }
  // return success();
  return failure();
}

LogicalResult tpu::MaxPoolWithMaskOp::BackwardH(int64_t &in_idx,
                                                int64_t &in_slice,
                                                int64_t out_idx,
                                                int64_t out_slice) {
  auto attr = parseParam();
  in_slice = (out_slice - 1) * attr.sh + attr.kh;
  in_idx = out_idx * attr.sh - attr.pad_h;
  bool is_last = (out_idx + out_slice == attr.oh);
  LocalGenInterface::fixSlice(in_idx, in_slice, attr.ih, is_last);
  return success();
}
