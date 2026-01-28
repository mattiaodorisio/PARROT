# DeLI

```bash
git clone ...
cd DeLI
mkdir build && cd build
cmake ..
make
ctest
```

## TODO
- Check VEB performance
- Check and implement proper VEB universe size
- To work with bits we use a order preserving transformation from float to uint (see include/DeLI/utils.h). Does this affect the distribution?
- RHT construction (set size, maxV, minV appropriately)
- RHT proper implementations: sentinels to avoid to check the bounds, remove jumps etc...
- RHT scaling can be improved with fastdiv
- VEB does not support duplicates

