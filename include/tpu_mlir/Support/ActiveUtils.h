//===----------------------------------------------------------------------===//
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Support/MathUtils.h"

namespace tpu_mlir {

using activate_f = std::function<double(double)>;

activate_f getActivateFunc(tpu::ActiveMode mode, f64_array_t coeffs);
activate_f getActivateFunc(tpu::ActiveOp op);

} // namespace tpu_mlir
