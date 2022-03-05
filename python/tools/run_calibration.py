#!/usr/bin/env python3
##
# Copyright (C) Cristal Vision Technologies Inc.
# All Rights Reserved.
##
import re
import argparse
import pymlir

from calibration.kld_calibrator import ActivationCalibrator
from calibration.data_selector import DataSelector


def buffer_size_type(arg):
    try:
        val = re.match('(\d+)G', arg).groups()[0]
    except:
        raise argparse.ArgumentTypeError("must be [0-9]G")
    return val


if __name__ == '__main__':
    print("SOPHGO Toolchain {}".format(pymlir.module().version))
    parser = argparse.ArgumentParser()
    parser.add_argument('mlir_file', metavar='mlir_file', help='mlir file')
    parser.add_argument('--dataset', type=str, help='dataset for calibration')
    parser.add_argument('--data_list', type=str, help='Input list file contain all input')
    parser.add_argument('--input_num',
                        type=int,
                        required=True,
                        help='num of images for calibration')
    parser.add_argument('--histogram_bin_num',
                        type=int,
                        default=2048,
                        help='Specify histogram bin numer for kld calculate')
    parser.add_argument('-o', '--calibration_table', type=str, help='output threshold table')
    args = parser.parse_args()

    selector = DataSelector(args.dataset, args.input_num, args.data_list)
    selector.dump('{}_calibration_list.txt'.format(args.calibration_table.split('.')[0]))

    # calibration
    calibrator = ActivationCalibrator(args.mlir_file,
                                      selector.data_list,
                                      args.histogram_bin_num)
    calibrator.run(args.calibration_table)

