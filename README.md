# A simple OpenCL runtime

This is a simple OpenCL runtime. It is purpose built for offloading parts of a
video codec onto a processor-integrated GPU. Therefore its supported OpenCL
features and capabilities are very limited.

Currently it is limited to Intel (R) GPUs. It uses Intel (R)'s
intel-graphics-compiler ("IGC") for generating GPU binary code out of OpenCL
source code.

It has the following main features:

  * Directly write to the framebuffer (or the backbuffer of the latter,
    respectively)

  * An offline compiler (but an online compiler can optionally be enabled during
    compilation of the runtime - this will however add a runtime dependency on
    IGC to programs that use the runtime)

Currently only processor integrated Intel (R) GPUs of the Gen9 (including
Gen9LP) architectures are supported.
