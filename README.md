# Toy RISCV Kernel

## Documents

See the [overview document](docs/overview.md) for more information.

## Build & Test

*The working directory is assumed to be the root of the project.*

### Build

You may use the following command to build the kernel:
```bash
$ make kernel
```

### Run

You may use the following command to run the kernel:
```bash
$ make qemu
```

### Test

*If you want to run a program on this kernel, see [the document for user](docs/user.md) for more detail.*

If you want to specify a certain program as the init program, you may
put the prgram in `user/` directory and try:
```bash
$ make qemu init=<name>
```

For example, if you want to run `user/hello.c`, you may try:
```bash
$ make qemu init=hello
```
