#!/usr/bin/env python3
import abc
import numpy as np
import argparse
from transform.OnnxConverter import OnnxConverter
from transform.BaseConverter import BaseConverter
from utils.mlir_shell import *
from tools.model_runner import mlir_inference, onnx_inference
import pymlir

class ModelTransformTool(object):

    def __init__(self, model_name):
        self.model_name = model_name
        self.converter = BaseConverter()

    def cleanup(self):
        pass

    def model_transform(self, mlir_file: str):
        self.mlir_file = mlir_file
        mlir_origin = mlir_file.replace('.mlir', '_origin.mlir', 1)
        self.converter.generate_mlir(mlir_origin)
        ret = mlir_opt(mlir_origin, self.mlir_file)
        if ret != 0:
            raise RuntimeError("mlir graph optimize fail")
        print("Mlir file generated:{}".format(mlir_file))

    def model_validate(self, file_list: str, tolerance, excepts):
        in_f32_npz = self.model_name + '_in_f32.npz'
        inputs = dict()
        if len(file_list) == 1 and file_list[0].endswith('.npz'):
            npz_in = np.load(file_list[0])
            for name in self.converter.input_names:
                assert (name in npz_in.files)
                inputs[name] = npz_in[name]
        else:
            assert (len(file_list) == len(self.converter.input_names))
            for name, file in zip(self.converter.input_names, file_list):
                assert (file.endswith('.npy'))
                inputs[name] = np.load(file)
        np.savez(in_f32_npz, **inputs)

        # original model inference to get blobs of all layer
        ref_outputs = self.model_inference(inputs)
        ref_npz = self.model_name + '_ref_outputs.npz'
        np.savez(ref_npz, **ref_outputs)

        # inference of mlir model
        f32_outputs = mlir_inference(inputs, self.mlir_file)
        f32_npz = self.model_name + '_f32_outputs.npz'
        np.savez(f32_npz, **f32_outputs)

        # compare all blobs layer by layers
        ret = f32_blobs_compare(f32_npz, ref_npz, tolerance, excepts=excepts)
        if ret != 0:
            raise RuntimeError("validate fail")

    @abc.abstractmethod
    def model_inference(self, inputs: dict) -> dict:
        pass


class OnnxModelTransformTool(ModelTransformTool):

    def __init__(self, model_name, model_def, input_shapes: list = []):
        super().__init__(model_name)
        self.model_def = model_def
        self.input_shapes = input_shapes
        self.converter = OnnxConverter(self.model_name, self.model_def, self.input_shapes)

    def model_inference(self, inputs: dict):
        return onnx_inference(inputs, self.converter.onnx_file)


def str2shape(v):
    _shape = eval(v)
    if not isinstance(_shape, list):
        raise KeyError("not shape list:{}".format(v))
    if len(_shape) == 0:
        return []
    dim = np.array(_shape).ndim
    if dim == 1:
        return [_shape]
    if dim != 2:
        raise KeyError("not shape list:{}".format(v))
    return _shape


def str2list(v):
    files = v.split(',')
    files = [s.strip() for s in files]
    return files


if __name__ == '__main__':
    print("SOPHGO Toolchain {}".format(pymlir.module().version))
    parser = argparse.ArgumentParser()
    parser.add_argument("--model_name", required=True, help="model name")
    parser.add_argument("--model_type", required=True, choices=['onnx'], help="model_type")
    parser.add_argument("--model_def", required=True, help="model definition file.")
    parser.add_argument("--model_data", help="caffemodel, only for caffe model")
    parser.add_argument("--input_shapes",
                        type=str2shape,
                        default=list(),
                        help="list of input shapes, like:[[2,3],[1,2]]")
    parser.add_argument("--input",
                        default=None,
                        type=str2list,
                        help="input npy/npz file for inference, "
                        "if has more than one input, join npy with semicolon")
    parser.add_argument("--tolerance",
                        default='0.99,0.99',
                        help="minimum similarity tolerance to model transform")
    parser.add_argument("--excepts", default='-', help="excepts")
    parser.add_argument("--mlir", type=str, required=True, help="output mlir model file")
    args = parser.parse_args()
    if not args.mlir.endswith('.mlir'):
        raise RuntimeError("your mlir file should endswith .mlir, not:{}".format(args.mlir))
    tool = None
    if args.model_type == 'onnx':
        tool = OnnxModelTransformTool(args.model_name, args.model_def, args.input_shapes)
    else:
        # TODO: support more AI model types
        raise RuntimeError("unsupport model type:{}".format(args.model_type))
    tool.model_transform(args.mlir)
    if args.input:
        tool.model_validate(args.input, args.tolerance, args.excepts)
    tool.cleanup()
