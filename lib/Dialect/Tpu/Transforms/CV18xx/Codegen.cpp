//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Backend/CV18xx/CV18xx.h"
#include "tpu_mlir/Dialect/Tpu/Transforms/CV18xx/MlirToCvimodel.hpp"
#include "tpu_mlir/Dialect/Tpu/Transforms/Passes.h"
#include "tpu_mlir/Support/Module.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include <fstream>
#include <set>
#include <sstream>

using namespace llvm;
using namespace tpu_mlir::backend;

using namespace flatbuffers;
namespace tpu_mlir {
namespace tpu {

class CVCodegenPass : public CVCodegenBase<CVCodegenPass> {
public:
  CVCodegenPass() {}
  void runOnOperation() override {
    module = getOperation();
    assert(module::isState(module::State::TPU_ADDRESSED));
    assert(module::isCV18xx());
    std::string filename = this->model_file;
    if (filename.empty()) {
      llvm_unreachable("output filename is empty");
    }
    CviModelBuilder builder(module);
    builder.storeModel(filename);
  }

private:
  ModuleOp module;
  StringRef chip;
};

std::unique_ptr<OperationPass<ModuleOp>> createCVCodegenPass() {
  return std::make_unique<CVCodegenPass>();
}

} // namespace tpu
} // namespace tpu_mlir
