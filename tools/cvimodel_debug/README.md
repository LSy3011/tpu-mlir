### cvimodel_debug usage

This tool can outputs all ops' results including the local ops in layer group and compares the similarity with the reference npz file. Take the mobilenet_v2 as the example,the usage is as follows:

```
 cvimodel_debug mobilenet_v2_cv183x_int8_sym.cvimodel mobilenet_v2_cv183x_int8_sym_tensor_info.txt mobilenet_v2_in_f32.npz output_int8.npz mobilenet_v2_cv183x_int8_sym_tpu_outputs.npz 0.99,0.99
```

Paramter1: xxx.cvimodel.

Paramter2: xxx_tensor_info.txt, generated by codegen pass.

Paramter3: input.npz.

Paramter4: output.npz.

Paramter5: reference.npz, compared with the debug output.npz.

Paramter6: tolerance, default as 0.99,0.99.

The detailed introduction can be acquired here:(https://wiki.xx.com/pages/viewpage.action?pageId=93259225)


