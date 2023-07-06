# clox

## Description
This repository contains the source code of clox, a programming language developed by [Bob Nystrom](https://github.com/munificent) for
his book - [Crafting Interpreters](https://craftinginterpreters.com). Although, the code here does follow the original one for
the most part, it still can (and certainly will) deviate at some points because this is an experimental build used to gain a better insight
into compilers history and internals. Some things will break as the time goes on.

If you want to try out the bytecode vm executable, there are no pre-built binaries as of yet, so the vm has to be built from source.

## Prerequisites
1. git
2. An up-to-date C/C++ compiler (gcc or clang)
3. [cmake](https://cmake.org)

## Build
1. Clone the repository:
```
git clone https://github.com/pleskunov/clox.git
```

2. Enter the project's directory and run a configuration script shipped with the code:
```
cd clox && ./configure.sh
```

3. Run a build script to build the clox from source code:
```
./built.sh
```

That's all it takes. Now, executable can be found in the *build* directory created by cmake.

## Lox syntax
This is in early stage, so only limited number of features are implemented. This section will be updated.

For now, only *numbers* (integers and doubles), some *unary* (-, !) and *binary* operators (+, -, *, /, >, >=, <, <=, ==), 
booleans (true, false), *nil* and *"strings"* are supported.

## Possible problems
1. If the version of cmake installed on your system is < 3.26, but > 3.22, you can safely "downgrade" the required version in the cmake file.
