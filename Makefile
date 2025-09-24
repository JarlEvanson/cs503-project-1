
CFLAGS := -Wall -Wextra --std=c99 -g -I include -c

.PHONY: build-lisp build-test build-fuzz
build-lisp: build/lisp
build-test: build/test-runner
build-fuzz: build/fuzz/fuzz-primary build/fuzz/fuzz-cmplog build/fuzz/fuzz-sanitizer

.PHONY: all
all: build-lisp build-test build-fuzz

.PHONY: test
test: build-test
	./build/test-runner

.PHONY: fuzz
fuzz: build-fuzz
	tmux new-session -d
	tmux new-window 'AFL_AUTORESUME=1 AFL_FINAL_SYNC=1 AFL_IMPORT_FIRST=1 afl-fuzz -i fuzz/inputs/ -o fuzz/tmp -M primary -- build/fuzz/fuzz-primary'
	tmux split-window 'AFL_AUTORESUME=1 AFL_FINAL_SYNC=1 AFL_IMPORT_FIRST=1 afl-fuzz -i fuzz/inputs/ -o fuzz/tmp -S sanitizer -- build/fuzz/fuzz-sanitizer'
	tmux split-window 'AFL_AUTORESUME=1 AFL_FINAL_SYNC=1 AFL_IMPORT_FIRST=1 afl-fuzz -i fuzz/inputs/ -o fuzz/tmp -S cmplog -c build/fuzz/fuzz-cmplog -- build/fuzz/fuzz-cmplog'
	tmux select-layout tiled
	tmux attach-session

build/lisp: $(patsubst %,build/executable/%, main.o $(OBJECTS))
	clang $^ -o $@

build/test-runner: $(patsubst %,build/test/%, test.o $(OBJECTS))
	clang -DENABLE_TESTS $^ -o $@

build/fuzz/fuzz-primary: $(patsubst %,build/fuzz/primary/%, fuzz.o $(OBJECTS))
	afl-clang-lto $^ -o $@

build/fuzz/fuzz-cmplog: $(patsubst %,build/fuzz/cmplog/%, fuzz.o $(OBJECTS))
	AFL_LLVM_CMPLOG=1 afl-clang-lto $^ -o $@

build/fuzz/fuzz-sanitizer: $(patsubst %,build/fuzz/sanitizer/%, fuzz.o $(OBJECTS))
	AFL_USE_ASAN=1 AFL_USE_UBSAN=1 AFL_USE_CFISAN=1 afl-clang-lto $^ -o $@

.PHONY: clean
clean:
	rm -rf build/

.PHONY: clean-all
clean-all: clean
	rm -rf fuzz/tmp/*

# Disable implicit build rules
%.o: %.c

build/executable/%.o: src/%.c include/
	mkdir -p $(dir $@)
	clang $(CFLAGS) $< -o $@

build/test/%.o: src/%.c include/
	mkdir -p $(dir $@)
	clang $(CFLAGS) -DENABLE_TESTS $< -o $@

build/fuzz/primary/%.o: src/%.c include/
	mkdir -p $(dir $@)
	afl-clang-lto $(CFLAGS) $< -o $@

build/fuzz/cmplog/%.o: src/%.c include/
	mkdir -p $(dir $@)
	AFL_LLVM_CMPLOG=1 afl-clang-lto $(CFLAGS) $< -o $@

build/fuzz/sanitizer/%.o: src/%.c include/
	mkdir -p $(dir $@)
	AFL_USE_ASAN=1 AFL_USE_UBSAN=1 AFL_USE_CFISAN=1 afl-clang-lto $(CFLAGS) $< -o $@
