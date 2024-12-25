# Dfs

## Getting started

Read some documentation about the game [here](DOCS.md)

### Compilation

Generate the protocol files by using the `gen_proto.sh` script

```bash
./gen_protos.sh
```

Compile with

```bash
cmake \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_ASAN=OFF \
    -DENABLE_TESTS=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    .
make -j$(nproc)
```

### Game files

You will need some game files. Go to [data/](data) and get the latest data.

## Running

Run the `./dfs` binary to create a proxy then lauch the game with the hook to `connect`.

## Hooking

### Building

Build the hook to the `connect` function using:

```bash
gcc -shared -fPIC -o hook.so hook.c -ldl
```

### Injecting

You can inject the hook using `LD_PRELOAD` on the launcher:

```bash
LD_PRELOAD=/path/to/hook.so ./Dofus_3.0-x86_64.AppImage
```

## Protocol

The beta did include the protocol descriptors, but Ankama woke up and decided to
remove the symbols from their binaries (they should have done this way earlier).

The protocols included in this repository were extracted from a previous
unobfuscated version.

The current method to extract the `.proto` files is to use some tool like `IL2CppDumper`
and match the messages with the older ones. This is quite a tedious task.

## Thanks

- [DofusInvoker](https://github.com/scalexm/DofusInvoker/tree/master)
