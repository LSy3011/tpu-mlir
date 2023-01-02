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
#include "tpu_mlir/Support/Float16.h"
#include "tpu_mlir/Support/Module.h"

#include "tpu_mlir/Support/MathUtils.h"

LogicalResult tpu::LeakyReluOp::init(InferenceParameter &p) {
  return success();
}
void tpu::LeakyReluOp::deinit(InferenceParameter &p) {}

LogicalResult tpu::LeakyReluOp::inference(InferenceParameter &p) {
  int64_t num_elements = module::getNumElements(getInput());
  memset(p.outputs[0], 0, sizeof(float) * num_elements);
  auto out_type = module::getStorageType(getOutput());
  auto asym = module::isAsymmetric();
  bool is_cv18xx = module::isCV18xx();

  if (out_type.isa<FloatType>()) {
    float *src = p.inputs[0];
    float *dst = p.outputs[0];
    float alpha = static_cast<float>(getAlpha().value().convertToDouble());
#pragma omp parallel for schedule(static, omp_schedule(num_elements))
    for (int64_t i = 0; i < num_elements; ++i) {
      dst[i] = src[i] > 0 ? src[i] : (alpha * src[i]);
    }
    if (out_type.isF16()) {
      F16(dst, dst, num_elements);
    } else if (out_type.isBF16()) {
      BF16(dst, dst, num_elements);
    }
  } else if (asym == false) {
    int64_t scale_neg, shift_neg;
    int64_t scalei = getMultiplier().value();
    int64_t shifti = getRshift().value();
    bool do_pos_scale = false;
    if (is_cv18xx) {
      scale_neg = getMultiplierNeg().value();
      shift_neg = getRshiftNeg().value();
      do_pos_scale = scalei != 0 ? true : false;
    }

#pragma omp parallel for schedule(static, omp_schedule(num_elements))
    for (int64_t i = 0; i < num_elements; ++i) {
      int64_t dst = 0;
      int64_t src = static_cast<int64_t>(p.inputs[0][i]);
      if (is_cv18xx) {
        if (src >= 0) {
          dst = do_pos_scale
                    ? applyMultiplierAndRShift(src, scalei, shifti, CVI_QUANT)
                    : src;
        } else {
          dst = applyMultiplierAndRShift(src, scale_neg, shift_neg, CVI_QUANT);
        }
      } else {
        dst = src >= 0 ? src : applyMultiplierAndRShift(src, scalei, shifti);
      }
      p.outputs[0][i] =
          out_type.isUnsignedInteger(8) ? to_uint8(dst) : to_int8(dst);
    }
  } else {
#pragma omp parallel for schedule(static, omp_schedule(num_elements))
    for (int64_t i = 0; i < num_elements; ++i) {
      int64_t src = static_cast<int64_t>(p.inputs[0][i]);
      int64_t dst = 0;

      auto i_qtype = module::getUniformQuantizedType(getInput());
      auto o_qtype = module::getUniformQuantizedType(getOutput());
      double scale = i_qtype.getScale() / o_qtype.getScale();
      dst = src >= i_qtype.getZeroPoint()
                ? src
                : ((src - i_qtype.getZeroPoint()) * (float)(1.0 / scale) +
                   o_qtype.getZeroPoint());

      p.outputs[0][i] =
          out_type.isUnsignedInteger(8) ? to_uint8(dst) : to_int8(dst);
    }
  }
  return success();
}
