#!/usr/bin/env python3
# ==============================================================================
#
# Copyright (C) 2022  All rights reserved.
#
# TPU-MLIR is licensed under the 2-Clause BSD License except for the
# third-party components.
#
# ==============================================================================

import argparse
import pymlir
import numpy as np
from pathlib import Path
from calibration.sensitive_layer import SensitiveLayer
from calibration.data_selector import DataSelector

if __name__ == '__main__':
    print("TPU-MLIR {}".format(pymlir.__version__))
    # yapf: disable
    parser = argparse.ArgumentParser(description="Search sensitive layer")
    parser.add_argument('mlir_file', help='fp32 mlir file')
    parser.add_argument('--dataset', help='dataset path for sensitive layer searching')
    parser.add_argument("--data_list", help="specify a file with inputs's absolute path for sensitive layer searching")
    parser.add_argument('--input_num', type=int, default=10,
                        help='num of inputs for quantization searching')
    parser.add_argument('--inference_num', type=int, default=10,
                        help='num of inputs for inference during sensitive layer searching')
    parser.add_argument('--max_float_layers', type=int, default=5,
                        help='num of maximum float layers')
    parser.add_argument('--tune_list', type=str, default='', help='Tune list file contain all input for tune')
    parser.add_argument('--tune_num', type=int, default=5, help='num of images for tune')
    parser.add_argument('--cali_method', type=str, default='use_kl',help='method of calibration')
    parser.add_argument('--expected_cos', type=float, default=0.99,
                        help='expected net output cos')
    parser.add_argument('--global_compare_layers', type=str, default='',
                        help='global compare layers, for example:\'layer1,layer2\' or \'layer1:0.3,layer2:0.7\'')
    parser.add_argument('--calibration_table', required=True,
                        help='calibration table generated by calibration or tune tool')
    parser.add_argument('--chip', '--processor', required=True, type=str,
                        choices=['bm1684x', 'bm1684', 'bm1688', 'cv183x', 'cv182x', 'cv181x', 'cv180x', 'cv186x', 'bm1690', 'mars3', 'sgtpuv8'],
                        help='chip platform name')
    parser.add_argument('--fp_type', default='auto', type=str,
                        choices=['auto', 'F16', 'F32', 'BF16'],
                        help='float type of mix precision')
    parser.add_argument('--histogram_bin_num', type=int, default=2048,
                        help='Specify histogram bin numer for kld calculate')
    parser.add_argument('--post_process', type=str, default=None,
                        help='post_process program path')
    parser.add_argument('-o', '--quantize_table', required=True,
                        help='output searched sensitive layers table')
    parser.add_argument('--debug_cmd', type=str, default='', help='debug cmd')

    # yapf: enable
    args = parser.parse_args()
    selector = DataSelector(args.dataset, args.input_num, args.data_list)
    tune_ds = None
    if args.tune_list:
        tune_ds = DataSelector(None, args.tune_num, args.tune_list)
        args.tune_num = len(tune_ds.data_list)
    searcher = SensitiveLayer(args, selector, tune_ds)
    searcher.run()
