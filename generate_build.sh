mkdir build

conan install . \
  --output-folder=build/relwithdebinfo \
  --build=missing \
  -s build_type=RelWithDebInfo

cmake -S . -B build/relwithdebinfo \
  -DCMAKE_TOOLCHAIN_FILE=build/relwithdebinfo/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -G Ninja

cmake --build build/relwithdebinfo -j

ctest --test-dir build/relwithdebinfo --output-on-failure

ln -s tpx3.tpx3 build/relwithdebinfo/tpx3.tpx3

conan install . \
  --output-folder=build/debug \
  --build=missing \
  -s build_type=Debug

cmake -S . -B build/debug \
  -DCMAKE_TOOLCHAIN_FILE=build/debug/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Debug -G Ninja

cmake --build build/debug -j

ln -s tpx3.tpx3 build/debug/tpx3.tpx3

ctest --test-dir build/debug --output-on-failure
