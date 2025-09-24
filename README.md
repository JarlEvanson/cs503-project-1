# Project 1 - Lisp

## Project Description

The project provides an implementation of a Lisp interpreter.

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
the `lisp` program.

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
```

To fuzz the interpreter, run:
```bash
make fuzz
```
