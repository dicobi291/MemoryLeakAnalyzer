# MemoryLeakAnalyzer
Static analyzer that find memory leaks

Build instruction:
  - install cmake https://cmake.org/download/
  - install python https://www.python.org/downloads/
  - build libclang with the visual studio:
    - git clone https://github.com/llvm/llvm-project.git
    - cd llvm-project
    - mkdir build (in-tree build is not supported)
    - cd build
    - cmake -DLLVM_ENABLE_PROJECTS=clang -G "Unix Makefiles" ../llvm
    - make
    - open llvm solution and build libclang project in libraries solution
  - create visual studio solution and add all files, except FileForParse directory
  - copy from <llvm-project-dir>/build/Debug(or Release)/bin libclang.dll to root directory of solution
  - add to solution properties path to additional include files: <llvm-project-dir>/clang/include
  - add to solution properties path to additional library directories: <llvm-project-dir>/build/Debug/lib
  - add to solution properties path to additional dependence: <llvm-project-dir>/build/Debug/lib/libclang.lib
