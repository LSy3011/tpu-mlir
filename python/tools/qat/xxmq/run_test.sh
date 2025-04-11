#!/bin/bash
# PTQ test
python ./tt_mq/ptq_train_all_model.py

# QAT test
python ./tt_mq/qat_train_all_model.py
