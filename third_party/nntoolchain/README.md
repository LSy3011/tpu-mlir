**注意：所有提交到代码库的第三方依赖库都必须是release版本的；nntoolchain和libsophon需要与tpu-mlir同级目录**

## TPU1684 2023-11-24
sha256: 700029cfcfbf5ba5d94f1d2d81abd7e0483eaa06

``` bash
cd  nntoolchain/net_compiler/
source  scripts/envsetup.sh
**debug: rebuild_bm1684_backend_cmodel**
**release: rebuild_bm1684_backend_release_cmodel**
cp out/install/lib/libcmodel_1684.so /workspace/tpu-mlir/third_party/nntoolchain/lib/
cp bmcompiler/libbackend/libbackend_1684.so /workspace/tpu-mlir/third_party/nntoolchain/lib/
```

## TPU1684X/1688/BM1690 2024-4-09
``` bash

#bm1684x sha256: b5877c2de29f96bee2e1778c61d58d59c5344665
cd TPU1686
source  scripts/envsetup.sh bm1684x
**debug: rebuild_backend_lib_cmodel**
**release: unset EXTRA_CONFIG && rebuild_backend_lib_release_cmodel**
cp build/backend_api/libbackend_bm1684x.so  /workspace/tpu-mlir/third_party/nntoolchain/lib/libbackend_1684x.so
cp build_runtime/firmware_core/libcmodel_firmware.so  /workspace/tpu-mlir/third_party/nntoolchain/lib/libcmodel_1684x.so
rebuild_firmware
cp build/firmware_core/libfirmware_core.so /workspace/tpu-mlir/third_party/nntoolchain/lib/libbm1684x_kernel_module.so
cp build/firmware_core/libfirmware_core.a /workspace/tpu-mlir/third_party/nntoolchain/lib/libbm1684x_kernel_module.a

#bm1688 sha256: 8454bd9b82900b0d146cdefa2c659fa890a135dc
cd TPU1686
source  scripts/envsetup.sh bm1686
**debug: rebuild_backend_lib_cmodel**
**release: unset EXTRA_CONFIG && rebuild_backend_lib_release_cmodel**
cp build/backend_api/libbackend_bm1686.so  /workspace/tpu-mlir/third_party/nntoolchain/lib/libbackend_1688.so
cp build_runtime/firmware_core/libcmodel_firmware.so  /workspace/tpu-mlir/third_party/nntoolchain/lib/libcmodel_1688.so
rebuild_firmware
cp build/firmware_core/libfirmware_core.so /workspace/tpu-mlir/third_party/nntoolchain/lib/libbmtpulv60_kernel_module.so
cp build/firmware_core/libfirmware_core.a /workspace/tpu-mlir/third_party/nntoolchain/lib/libbmtpulv60_kernel_module.a

#bm1690 sha256: 8454bd9b82900b0d146cdefa2c659fa890a135dc
cd TPU1686
source  scripts/envsetup.sh sg2260
**debug: rebuild_backend_lib_cmodel**
**release: unset EXTRA_CONFIG && rebuild_backend_lib_release_cmodel**
cp build/backend_api/libbackend_sg2260.so /workspace/tpu-mlir/third_party/nntoolchain/lib/libbackend_bm1690.so
cp build_runtime/firmware_core/libcmodel_firmware.so /workspace/tpu-mlir/third_party/nntoolchain/lib/libcmodel_bm1690.so

#sg2380 sha256: 2534dced91a0b67cb9525236e341bdc8aa9fba3b
cd TPU1686
source  scripts/envsetup.sh sg2380
**debug: rebuild_backend_lib_cmodel**
**release: unset EXTRA_CONFIG && rebuild_backend_lib_release_cmodel**
cp build/backend_api/libbackend_sg2380.so /workspace/tpu-mlir/third_party/nntoolchain/lib/libbackend_sg2380.so
cp build_runtime/firmware_core/libcmodel_firmware.so /workspace/tpu-mlir/third_party/nntoolchain/lib/libcmodel_sg2380.so
```


## tpu-runtime 2023-3-24
build from tpu-runtime 593e93f3489e60fc79dd9335d4bc151c8cadaf62
``` bash
pushd libsophon
mkdir -p build && cd build
cmake -G Ninja -DPLATFORM=cmodel -DCMAKE_BUILD_TYPE=DEBUG ../ # release version has problem
ninja
cp -P tpu-runtime/libbmrt.so* /workspace/tpu-mlir/third_party/nntoolchain/lib/
cp -P bmlib/libbmlib.so* /workspace/tpu-mlir/third_party/nntoolchain/lib/
popd
```


## tpu-cpuop 2023-10-10

```bash
pushd /workspace/nntoolchain/net_compiler
source new_scripts/envsetup.sh
rebuild_cpuop
cp /workspace/nntoolchain/net_compiler/out/lib/libcpuop.so* /workspace/tpu-mlir/third_party/nntoolchain/lib/

# libbmcpu.so/libusercpu.so are deprecated
```
