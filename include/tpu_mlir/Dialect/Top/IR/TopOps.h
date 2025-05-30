//===----------------------------------------------------------------------===//
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"
#include "tpu_mlir/Dialect/Top/IR/TopOpsDialect.h.inc"
#include "tpu_mlir/Interfaces/FlopsInterface.h"
#include "tpu_mlir/Interfaces/InferenceInterface.h"
#include "tpu_mlir/Interfaces/ShapeInterface.h"
#include "tpu_mlir/Support/AttrStruct.h"
#include "tpu_mlir/Support/TensorFile.h"
#include "tpu_mlir/Traits/Traits.h"
#include "llvm/ADT/TypeSwitch.h"
#define GET_ATTRDEF_CLASSES
#include "tpu_mlir/Dialect/Top/IR/TopAttr.h.inc"
#define GET_OP_CLASSES
#include "tpu_mlir/Dialect/Top/IR/TopOps.h.inc"
