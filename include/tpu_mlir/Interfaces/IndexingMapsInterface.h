//===----------------------------------------------------------------------===//
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "mlir/IR/OpDefinition.h"
/// Include the ODS generated interface header files.
#include "tpu_mlir/Interfaces/IndexingMapsInterface.h.inc"

namespace tpu_mlir {

mlir::ArrayAttr getBinaryIndexingMaps(mlir::Operation *op);
mlir::AffineMap getBinaryMap(const mlir::AffineMap &refer_map,
                             const llvm::ArrayRef<int64_t> &shape);

} // namespace tpu_mlir
