//===----------------------------------------------------------------------===//
//
// Copyright (c) 2020-2030 by Sophgo Technologies Inc. All rights reserved.
//
// Licensed under the Apache License v2.0.
// See http://www.apache.org/licenses/LICENSE-2.0 for license information.
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#pragma once

#include "mlir/Support/LLVM.h"
#include "tpu_mlir/Backend/BM168x/BM168x.h"
#include "tpu_mlir/Dialect/Tpu/IR/TpuOps.h"
#include "tpu_mlir/Support/Module.h"
#include <list>
#include <map>
#include <set>

#include "tpu_mlir/Dialect/Tpu/Transforms/LayerGroup/LayerGroupDefs.h"
#include "tpu_mlir/Dialect/Tpu/Transforms/LayerGroup/SwPipeline.h"

namespace tpu_mlir {
namespace tpu {

class TimeStepMethod;

class BasicTimeStep {
public:
  BasicTimeStep();
  virtual ~BasicTimeStep() {}
  void clear();

  bool assignTimeStep(const LgInfo &lg_info, const shape_secs_t &shape_secs,
                      bool gen_idx);
  void add_tpu0_ts_field(const TpuTsField &field);
  void add_gdma0_ts_field(const GdmaTsField &field);
  void add_tpu0_gdma0_ts_field(const TpuTsField &tpu_field,
                               const GdmaTsField &gdma_field);
  void update_gdma0_ts_field(int64_t ts, const GdmaTsField &field);

  int64_t get_tensor_life_time(Value v);

  int64_t get_tensor_range_end(const GdmaElt &tensor, int64_t cur_ts);
  std::shared_ptr<SoftwarePipeline> get_timestep_swpipl() { return swpipl_; }
  int64_t get_layer_swpipl_stage(Operation *op);
  int64_t get_tensor_swpipl_stage(Value v);
  int64_t get_swpipl_stage_num() { return swpipl_stage_num_; }
  void set_swpipl_stage_num(int num) { swpipl_stage_num_ = num;} //just for ir gen
  void software_pipeline();

  // getter
  size_t get_timestep_num() { return timestep_table_.size(); }
  const TpuTsField &getLayers(int64_t ts) {
    return timestep_table_[ts].tpu0_ts_field;
  }
  const GdmaTsField &getTensors(int64_t ts) {
    return timestep_table_[ts].gdma0_ts_field;
  }
  int64_t get_lmem_addr(const mem_buffer_key_t &buffer_key);
  int64_t get_lmem_size(const mem_buffer_key_t &buffer_key);
  MemBuff &get_lmem_buffer() { return lmem_buffer_; }

  const mem_buffer_value_t &
  get_lmem_buffer_value(const mem_buffer_key_t &buffer_key);
  int64_t get_lmem_occupy() const { return lmem_occupy_; }
  std::map<Value, int64_t, value_compare> &get_hold_coeff() {
    return hold_coeff_;
  }

  TensorInfo &get_tensor_infos();

  // setter
  void set_lmem_addr(const mem_buffer_key_t &buffer_key, int64_t lmem_addr);
  void set_lmem_occupy(int64_t occupy) { lmem_occupy_ = occupy; }

  void gen_all_mem_buffer();
  void update_all_mem_buffer_size(const LgInfo &lg_info);
  void gen_hold_coeff();
  bool is_tensor_hold_in_lmem(Value v);
  void cancel_tensor_hold_in_lmem(Value v);

  // visualizer
  void show_timestep();
  void show_lmem_buffer();

protected:
  std::shared_ptr<TimeStepMethod> timestep_method_;
  std::shared_ptr<SoftwarePipeline> swpipl_;
  std::vector<TimestepRow> timestep_table_;

  std::map<Value, int64_t, value_compare> hold_coeff_;
  TensorInfo tensor_infos_;
  int64_t swpipl_stage_num_;

  int64_t lmem_occupy_;
  MemBuff lmem_buffer_;
};

using BasicTimeStepPtr = std::shared_ptr<BasicTimeStep>;

} // namespace tpu
} // namespace tpu_mlir
