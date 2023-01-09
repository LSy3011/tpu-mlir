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
#include <vector>

#include "tpu_mlir/Dialect/Tpu/Transforms/LayerGroup/BasicTimeStep.h"
#include "tpu_mlir/Dialect/Tpu/Transforms/LayerGroup/CycleCalculator.h"
#include "tpu_mlir/Dialect/Tpu/Transforms/LayerGroup/LayerGroupDefs.h"
#include "tpu_mlir/Dialect/Tpu/Transforms/LayerGroup/LgPass.h"
namespace tpu_mlir {
namespace tpu {

class TimeStepMethod {

public:
  TimeStepMethod() { cycle_calculator_ = std::make_shared<CycleCalculator>(); }
  ~TimeStepMethod(){};
  bool process(BasicTimeStep *time_step, TensorInfo &tensor_infos,
               const LgInfo &lg_info, const shape_secs_t &shape_secs,
               bool gen_idx);
  void layer_nearest_timestep_assignment(BasicTimeStep *time_step,
                                         TensorInfo &tensor_infos,
                                         const LgInfo &group_info);
  void memory_aware_timestep_assignment(BasicTimeStep *time_step,
                                        TensorInfo &tensor_infos,
                                        const LgInfo &group_info);

  void get_timestep_cycle_slack(
      BasicTimeStep *time_step, const LgInfo &lg_info,
      ValueIntMap &tensor_to_cycle,
      ValueIntMap &tensor_to_bufsize,
      std::vector<std::list<GdmaElt>> &tensor_timesteps,
      std::vector<int64_t> &timestep_cycle_slack);
  int64_t get_to_ts(bool &is_valid, int64_t cur_ts, TIMESTEP_LD_ST ld_st,
                    int64_t range_end);
  int64_t
  get_best_ts(BasicTimeStep *time_step, const LgInfo &lg_info, int64_t cur_ts,
              ValueIntMap &tensor_to_cycle,
              ValueIntMap &tensor_to_bufsize,
              std::vector<std::list<GdmaElt>> &tensor_timesteps,
              std::vector<int64_t> &timestep_cycle_slack,
              std::list<GdmaElt>::iterator &sel_list_iter);

  void bubble_tensor_to_best_ts(
      std::list<GdmaElt>::iterator sel_list_iter, int64_t cur_ts,
      int64_t best_ts, BasicTimeStep *time_step,
      ValueIntMap &tensor_to_cycle,
      ValueIntMap &tensor_to_bufsize,
      std::vector<std::list<GdmaElt>> &tensor_timesteps,
      std::vector<int64_t> &timestep_cycle_slack);

  //  void set_local_layer_param(Operation *op, int64_t nidx, int64_t nslice,
  //                             int64_t hidx, int64_t hslice);

private:
  std::shared_ptr<CycleCalculator> cycle_calculator_;
};

std::unique_ptr<LgPass> CreateTimeStepAssignmentPass();

} // namespace tpu
} // namespace tpu_mlir
