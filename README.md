# Project 1 - Lisp

## Project Description

The project provides an implementation of a Lisp interpreter.

## Special Notes

The project provides an implementation of a copying garbage collector and the
built-in functions `let` and `begin`. The project also implemented `funcall`,
and `function` in order to support higher-order functions in the Type-2
environment.

## Organization

The code is organized into a source directory (`src/`) and a header directory
(`include/`). Each component or activity required by the interpreter is located
in its own set of a `c` source file and a `c` header file and have the same
names.

For instance, `src/sexpr.c` and `src/sexpr.h` contain basic functionality to
initialize and manipulate the `SExpr` structure (the basic unit of the
interpreter). 

The entry point of the unit testing and integration testing programs are
located in `src/test.c`, while the entry point of the fuzzing programs are
located in `src/fuzz.c`.

The `fuzz/` directory contains the initial seeds for a fuzzing run, and will
contain a temporary directory when fuzzing.

The `test/` directory contains various assets used for integration testing of
the `lisp` program. Each sprint that has integration tests has a folder named
after it. The index file inside each sprint's folder defines the name of each
integration test, which has two files to specify input and output from the
interpreter. The files are respectively `name.i` and `name.o`. The input file
is run through the interpreter and compared with the parsed form of the output
file. If the evaluated input and the parsed output are the same, then the test
succeeds. Other tests are located at the bottom of source files and test various
other aspects of the interpreter that may not be well-tested or easily tested
using the integration tester.

The `submissions/` directory will contain assets related to the submission of
various sprints.

The `build/` directory will contain the object files and linked programs
produced by the build process.

## Build Process

### Prerequisites

In order to build `lisp`, the following programs must be installed:
`GNU make` and `gcc`.

The prerequisites to build the unit testing and integration testing programs
are identical to the ones required to build `lisp`.

In order to fuzz `lisp`, the following additional programs must be installed:
`afl++` and `tmux`.

### Building

To build `lisp`, run:
```bash
make
```

To build the unit testing and integration testing programs, run:
```bash
make build-test
```

To build the fuzzing programs, run:
```bash
make build-fuzz
```

## Testing

To run the unit and integration tests, run:
```bash
make test
./build/test-runner
```

To fuzz the interpreter, run:
```bash
make fuzz
```
