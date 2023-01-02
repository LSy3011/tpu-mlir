//===----------------------------------------------------------------------===//
//
// Copyright (c) 2020-2030 by Sophgo Technologies Inc. All rights reserved.
//
// Licensed under the Apache License v2.0.
// See http://www.apache.org/licenses/LICENSE-2.0 for license information.
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Dialect/Tpu/IR/TpuOps.h"
#include "tpu_mlir/Support/Dnnl/Dnnl.h"
#include "tpu_mlir/Support/Module.h"

#include "tpu_mlir/Support/LutFunc.h"
#include "tpu_mlir/Support/MathUtils.h"



gru_attr_t tpu::GRUOp::parseParam() {
  gru_attr_t attr = {0};
  auto in_shape = module::getShape(getInput());
  assert(in_shape.size() == 3);
  if (getBatchFirst()) {
    attr.batch_size = in_shape[0];
    attr.seq_len = in_shape[1];
    attr.batch_first = true;
  } else {
    attr.batch_size = in_shape[1];
    attr.seq_len = in_shape[0];
    attr.batch_first = false;
  }
  attr.num_direction = getBidirectional() ? 2 : 1;
  attr.hidden_size = getHiddenSize();
  attr.input_size = in_shape[2];
  attr.have_bias = !getBias().getType().isa<mlir::NoneType>();
  attr.have_h0 = !getInitialH().getType().isa<mlir::NoneType>();
  attr.output_y = !getY().getType().isa<mlir::NoneType>();
  attr.output_yh = !getYH().getType().isa<mlir::NoneType>();
  return attr;
}

static inline float sigmoid_(float x) { return 0.5 * tanh(0.5 * x) + 0.5; }

static inline float tanh_(float x) { return tanh(x); }

class BmGruInference {
public:
  static void inference(InferenceParameter &p, tpu::GRUOp *op) {
    auto attr = op->parseParam();
    auto h0_buffer = std::make_shared<std::vector<float>>(
        attr.num_direction * attr.batch_size * attr.hidden_size, 0.0f);
    auto initial_h = h0_buffer->data();
    if (attr.have_h0) {
      memcpy(initial_h, p.inputs[4], h0_buffer->size() * sizeof(float));
    }

    auto bias_buffer = std::make_shared<std::vector<float>>(
        attr.num_direction * 6 * attr.hidden_size, 0.0f);
    auto B = bias_buffer->data();
    if (attr.have_bias) {
      memcpy(B, p.inputs[3], bias_buffer->size() * sizeof(float));
    }

    gru_compute(p, attr, B, initial_h, true);
    if (attr.num_direction == 2) {
      gru_compute(p, attr, B, initial_h, false);
    }
  }

private:
  static void gru_compute(InferenceParameter &p, const gru_attr_t &attr,
                          float *bias, float *h, bool forward) {
    //(TODO) num_layers > 1
    // input += seq_length * batch * input_size * num_layer; //(TODO check!)
    float *input = p.inputs[0];
    float *output = p.outputs[0];
    float *x_wz = p.inputs[1];
    float *h_wz = p.inputs[2];
    float *x_bz = bias;
    float *last_h = p.outputs[1]; // Y_h
    if (!forward) {
      x_wz += 3 * attr.input_size * attr.hidden_size;
      x_bz += 2 * 3 * attr.hidden_size;
      h_wz += 3 * attr.hidden_size * attr.hidden_size;
      h += attr.batch_size * attr.hidden_size;
      output += attr.batch_size * attr.hidden_size;
      last_h += attr.batch_size * attr.hidden_size;
    }
    float *prev_hidden_state = h;
    float *x_wr = x_wz + attr.input_size * attr.hidden_size;
    float *x_wh = x_wr + attr.input_size * attr.hidden_size;

    float *x_br = x_bz + attr.hidden_size;
    float *x_bh = x_br + attr.hidden_size;

    float *h_wr = h_wz + attr.hidden_size * attr.hidden_size;
    float *h_wh = h_wr + attr.hidden_size * attr.hidden_size;

    float *h_bz = x_bz + 3 * attr.hidden_size;
    float *h_br = h_bz + attr.hidden_size;
    float *h_bh = h_br + attr.hidden_size;

    std::vector<float> x_z(attr.batch_size * attr.hidden_size);
    std::vector<float> x_r(attr.batch_size * attr.hidden_size);
    std::vector<float> x_h(attr.batch_size * attr.hidden_size);

    std::vector<float> h_z(attr.batch_size * attr.hidden_size);
    std::vector<float> h_r(attr.batch_size * attr.hidden_size);
    std::vector<float> h_h(attr.batch_size * attr.hidden_size);

    std::vector<float> g_z(attr.batch_size * attr.hidden_size);
    std::vector<float> g_r(attr.batch_size * attr.hidden_size);
    std::vector<float> g_h(attr.batch_size * attr.hidden_size);

    for (int s = 0; s < attr.seq_len; s++) {
      // matrixmul
      // x op w_x : [batch, x_size] op [x_size, attr.hidden_size] => [batch,
      // attr.hidden_size] h op w_h : [batch, h_size] op [h_size,
      // attr.hidden_size] => [batch, attr.hidden_size] note: h_size =
      // attr.hidden_size?
      int seq_idx = forward ? s : (attr.seq_len - s - 1);
      float *x = input + seq_idx * attr.batch_size * attr.input_size;

      dnnl_mm(x, x_wz, x_bz, x_z.data(), attr.batch_size, attr.input_size,
              attr.hidden_size, false);
      dnnl_mm(x, x_wr, x_br, x_r.data(), attr.batch_size, attr.input_size,
              attr.hidden_size, false);
      dnnl_mm(x, x_wh, x_bh, x_h.data(), attr.batch_size, attr.input_size,
              attr.hidden_size, false);

      dnnl_mm(prev_hidden_state, h_wz, h_bz, h_z.data(), attr.batch_size,
              attr.hidden_size, attr.hidden_size, false);
      dnnl_mm(prev_hidden_state, h_wr, h_br, h_r.data(), attr.batch_size,
              attr.hidden_size, attr.hidden_size, false);
      dnnl_mm(prev_hidden_state, h_wh, h_bh, h_h.data(), attr.batch_size,
              attr.hidden_size, attr.hidden_size, false);

      for (int batch = 0; batch < attr.batch_size; batch++) {
        float *xz = x_z.data() + batch * attr.hidden_size;
        float *xr = x_r.data() + batch * attr.hidden_size;
        float *xh = x_h.data() + batch * attr.hidden_size;

        float *hz = h_z.data() + batch * attr.hidden_size;
        float *hr = h_r.data() + batch * attr.hidden_size;
        float *hh = h_h.data() + batch * attr.hidden_size;
        float *pre_state = prev_hidden_state + batch * attr.hidden_size;
        float *hidden_state = pre_state;
        if (attr.output_y) {
          hidden_state =
              output +
              (seq_idx * attr.num_direction * attr.batch_size + batch) *
                  attr.hidden_size;
        }
        for (int i = 0; i < attr.hidden_size; i++) {
          hz[i] = sigmoid_(hz[i] + xz[i]);
          hr[i] = sigmoid_(hr[i] + xr[i]);
          hh[i] = tanh_(hr[i] * hh[i] + xh[i]);
          hidden_state[i] = (1 - hz[i]) * hh[i] + hz[i] * pre_state[i];
        }
      }
      if (attr.output_y) {
        prev_hidden_state = output + seq_idx * attr.num_direction *
                                         attr.batch_size * attr.hidden_size;
      }
    }
    if (attr.output_yh) {
      memcpy(last_h, prev_hidden_state,
             attr.batch_size * attr.hidden_size * sizeof(float));
    }
  }
};

class CvGruInference {
public:
  struct cv_gru_param_t {
    int seq_length;
    int hidden_size;
    int batch_size;
    int input_size;
    int num_dir;
    bool bidirectional;
    bool linear_before_reset;
    bool only_last;
    float *r_z;
    float *r_r;
    float *r_h;
    float *r_bz;
    float *r_br;
    float *r_bh;
    float *input;
    float *output;
    float *prev_hidden_state;
    float *sigmoid_lut;
    float *sigmoid_slope_lut;
    float *tanh_lut;
    float *tanh_slope_lut;
    uint32_t out_idx;
  };

  static void inference(InferenceParameter &p, tpu::GRUOp *op) {
    cv_gru_param_t gp;
    auto input_type = op->getInput().getType().dyn_cast<TensorType>();
    auto in_shape = module::getShape(op->getInput());
    Value output;
    for (uint32_t i = 0; i < op->getNumResults(); ++i) {
      if (!op->getResults()[i].getType().isa<mlir::NoneType>()) {
        output = op->getResults()[i];
        gp.out_idx = i;
        break;
      }
    }
    assert(output);
    auto out_shape = module::getShape(output);
    if (out_shape.size() == 4) {
      gp.seq_length = out_shape[0];
      gp.num_dir = out_shape[1];
      gp.batch_size = out_shape[2];
      gp.hidden_size = out_shape[3];
      assert(in_shape[0] == gp.seq_length);
      gp.only_last = false;
    } else {
      gp.seq_length = in_shape[0];
      gp.num_dir = out_shape[0];
      gp.batch_size = out_shape[1];
      gp.hidden_size = out_shape[2];
      gp.only_last = true;
    }
    assert(in_shape.size() == 3);
    assert(in_shape[1] == gp.batch_size);
    gp.input_size = in_shape[2];
    gp.linear_before_reset = op->getLinearBeforeReset();
    assert(gp.linear_before_reset == true);
    gp.bidirectional = op->getBidirectional();

    auto out_type = module::getStorageType(output);
    bool is_bf16 = out_type.isBF16();
    compute(true, p, gp, is_bf16);
    if (gp.bidirectional) {
      compute(false, p, gp, is_bf16);
    }
    if (is_bf16) {
      auto ele_num = module::getNumElements(output);
      BF16(p.outputs[gp.out_idx], p.outputs[gp.out_idx], ele_num, false);
    }
  }

private:
  static double cv_sigmoid(float data, bool is_bf16, cv_gru_param_t &gp) {
    if (is_bf16) {
      float var = data;
      bf16_lut_slope(&var, &var, 1, gp.sigmoid_lut, gp.sigmoid_slope_lut, -12,
                     12);
      return var;
    } else {
      return 0.5 * tanh(0.5 * data) + 0.5;
    }
  }
  static double cv_tanh(float data, bool is_bf16, cv_gru_param_t &gp) {
    if (is_bf16) {
      float var = data;
      bf16_lut_slope(&var, &var, 1, gp.tanh_lut, gp.tanh_slope_lut, -15, 15);
      return var;
    } else {
      return tanh(data);
    }
  }
  static void update_addr(bool forward, InferenceParameter &p,
                          cv_gru_param_t &gp) {
    if (forward) {
      gp.r_z = p.inputs[2];
      gp.r_bz = p.inputs[3];
      gp.output = p.outputs[gp.out_idx];
      gp.prev_hidden_state = p.inputs[4];
      gp.input = p.inputs[0];
    } else {
      gp.r_z = p.inputs[2] + 3 * gp.hidden_size * gp.hidden_size;
      gp.r_bz = p.inputs[3] + 3 * gp.hidden_size;
      gp.output = p.outputs[gp.out_idx] + gp.batch_size * gp.hidden_size;
      gp.prev_hidden_state = p.inputs[4] + gp.batch_size * gp.hidden_size;
      gp.input = p.inputs[0] + 3 * gp.hidden_size;
    }
    gp.r_r = gp.r_z + gp.hidden_size * gp.hidden_size;
    gp.r_h = gp.r_r + gp.hidden_size * gp.hidden_size;
    gp.r_br = gp.r_bz + gp.hidden_size;
    gp.r_bh = gp.r_br + gp.hidden_size;
    gp.sigmoid_lut = p.inputs[6];
    gp.sigmoid_slope_lut = p.inputs[7];
    gp.tanh_lut = p.inputs[8];
    gp.tanh_slope_lut = p.inputs[9];
  }

  static void compute(bool forward, InferenceParameter &p, cv_gru_param_t &gp,
                      bool is_bf16) {
    update_addr(forward, p, gp);
    std::vector<float> update_gate(gp.batch_size * gp.hidden_size); // zt
    std::vector<float> reset_gate(gp.batch_size * gp.hidden_size);  // rt
    std::vector<float> hidden_gate(gp.batch_size * gp.hidden_size); // ht

    for (int t = 0; t < gp.seq_length; ++t) {
      int seq_idx = forward ? t : (gp.seq_length - t - 1);
      // zt = sigmoid(Xt*(Wz^T) + Ht-1*(Rz^T) + Wbz + Rbz)
      // rt = sigmoid(Xt*(Wr^T) + Ht-1*(Rr^T) + Wbr + Rbr)
      // ht = tanh(Xt*(Wh^T) + (rt (.) (Ht-1*(Rh^T) + Rbh)) + Wbh)
      // H = (1-zt) * ht + zt * Ht
      float *xt = gp.input + seq_idx * gp.batch_size * gp.input_size;
      dnnl_mm(gp.prev_hidden_state, gp.r_z, gp.r_bz, update_gate.data(),
              gp.batch_size, gp.hidden_size, gp.hidden_size, false);
      dnnl_mm(gp.prev_hidden_state, gp.r_r, gp.r_br, reset_gate.data(),
              gp.batch_size, gp.hidden_size, gp.hidden_size, false);
      dnnl_mm(gp.prev_hidden_state, gp.r_h, gp.r_bh, hidden_gate.data(),
              gp.batch_size, gp.hidden_size, gp.hidden_size, false);
      if (is_bf16) {
        BF16(update_gate.data(), update_gate.data(), update_gate.size());
        BF16(reset_gate.data(), reset_gate.data(), reset_gate.size());
        BF16(hidden_gate.data(), hidden_gate.data(), hidden_gate.size());
      }
      for (int batch = 0; batch < gp.batch_size; batch++) {
        float *xz = xt + batch * gp.input_size;
        float *xr = xz + gp.hidden_size;
        float *xh = xr + gp.hidden_size;
        float *ug = update_gate.data() + batch * gp.hidden_size;
        float *rg = reset_gate.data() + batch * gp.hidden_size;
        float *hg = hidden_gate.data() + batch * gp.hidden_size;
        float *hidden_state = nullptr;
        float *pre_state = gp.prev_hidden_state + batch * gp.hidden_size;
        if (gp.only_last) {
          hidden_state = gp.output + batch * gp.hidden_size;
        } else {
          hidden_state =
              gp.output +
              (seq_idx * gp.num_dir * gp.batch_size + batch) * gp.hidden_size;
        }
#pragma omp parallel for schedule(static, omp_schedule(gp.hidden_size))
        for (int i = 0; i < gp.hidden_size; ++i) {
          if (is_bf16) {
            ug[i] = cv_sigmoid(BF16(ug[i] + xz[i]), is_bf16, gp);
            rg[i] = cv_sigmoid(BF16(rg[i] + xr[i]), is_bf16, gp);
            hg[i] = cv_tanh(BF16(BF16(rg[i] * hg[i]) + xh[i]), is_bf16, gp);
            hidden_state[i] = BF16(BF16(BF16(ug[i] * pre_state[i]) + hg[i]) -
                                   BF16(ug[i] * hg[i]));
          } else {
            ug[i] = cv_sigmoid(ug[i] + xz[i], is_bf16, gp);
            rg[i] = cv_sigmoid(rg[i] + xr[i], is_bf16, gp);
            hg[i] = cv_tanh(rg[i] * hg[i] + xh[i], is_bf16, gp);
            hidden_state[i] = (1 - ug[i]) * hg[i] + ug[i] * pre_state[i];
          }
        }
      }
      if (gp.only_last) {
        gp.prev_hidden_state = gp.output;
      } else {
        gp.prev_hidden_state =
            gp.output + seq_idx * gp.num_dir * gp.batch_size * gp.hidden_size;
      }
    }
  }
};

LogicalResult tpu::GRUOp::init(InferenceParameter &p) { return success(); }
void tpu::GRUOp::deinit(InferenceParameter &p) {}

LogicalResult tpu::GRUOp::inference(InferenceParameter &p) {
  if (module::isCV18xx()) {
    CvGruInference::inference(p, this);
  } else {
    BmGruInference::inference(p, this);
  }
  return success();
}
