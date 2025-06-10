# Jamcore
Flexible and fast audio programming engine.
Currently just a toy project.

Only works on Mac right now. Plans to support ALSA on Linux.
Requires clang compiler with C11 support.

```
# Make library and example
make 

# Run the example
make run

# Debug the example with lldb
make debug

# Build with address santizer enabled
make SAN=asan

# Build with thread santizer enabled
make SAN=tsan
```
