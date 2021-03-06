tests := $(patsubst test_%.cpp, %, $(wildcard test_*.cpp))

mode ?= debug

show_mode_and_goals := $(shell >&2 printf -- "\e[4m%s: [%s]\e[0m\n" "$$(echo $(mode) | tr [:lower:] [:upper:] )" "$(MAKECMDGOALS)")

# Compiler to use
CXX ?= g++
# Proxy which prettifies g++ output if prettifier is found, and deletes output file if compilation failed
define_cc_proxy = function cc_proxy {( set -euo pipefail; echo "$(CXX) $$*"; $(CXX) 2>&1 "$$@" | ( which c++-color &>/dev/null && c++-color || cat ) || { rm -f -- "$@"; false; }; )}

cc := cc_proxy

cc_base := -pipe -pedantic -std=c++14
ld_base := -pipe -lpthread

ifeq ($(mode),debug)
cc_opts := $(cc_base) -Wall -Wextra -Wno-unused-parameter -Og -g -DDEBUG
ld_opts := $(ld_base) -Wall
else
ifeq ($(mode),release)
cc_opts := $(cc_base) -Wall -Werror -flto -fPIC -O3
ld_opts := $(ld_base) -Wall -Werror -flto -s
else
$(error "Unknown mode: %(mode)")
endif
endif

ifneq ($(nowarn),)
cc_opts := $(filter-out -Wall, $(cc_opts)) -w
endif

test := test
dep := dep/$(mode)
out := out/$(mode)
obj := obj/$(mode)

outdirs := test/ dep/ out/ obj/

.PHONY: default syntax clean list-deps stats tests

.SECONDARY:

.ONESHELL:

.SHELLFLAGS: -euo pipefail -c

.DEFAULT: tests

syntax:
	@$(define_cc_proxy)
	$(cc) $(cc_base) -Wall -fsyntax-only $(filter-out test_%, $(wildcard *.cpp))

clean:
	rm -rf -- $(outdirs)

# Run all tests

tests: $(tests:%=test/%)
	@for test in $^; do
		printf -- "Running test: '%s'\n" "$${test}"
		"$${test}" --test-silent-if-perfect
	done

tests-loud: $(tests:%=test/%)
	@for test in $^; do
		printf -- "Running test: '%s'\n" "$${test}"
		"$${test}"
	done

# Fun

list-deps:
	@perl -ne 'print $$1."\n" if m/#include <([^>]+)>/' *.h *.cpp *.tcc | sort | uniq

stats:
	@(
		printf -- "Lines\tChars\tUnit name\n"
		for root in $$(ls *.{cpp,tcc,h} | sed -e 's/\..*$$//g' | sort -u); do
			printf -- "%s\t%s\t%s\n" $$(cat $${root}.* | wc -cl) "$${root}"
		done | sort -rnt$$'\t';
	) | column -t -o' | ' -s$$'\t'

# Directories

$(test) $(dep) $(out) $(obj):
	mkdir -p $@

# Object files and autodependencies

$(obj)/%.o: %.cpp | $(obj) $(dep)
	@$(define_cc_proxy)
	$(cc) $(cc_opts) -x c++ $< -MMD -MF $(dep)/$*.d -MQ $@ -c -o $@

# Projects

$(out)/%: $(obj)/%.o | $(out)
	@$(define_cc_proxy)
	$(cc) $(ld_opts) $^ -o $@

# Test dependencies

$(test)/promise: $(obj)/promise.o

$(test)/event_loop: $(obj)/event_loop.o $(obj)/starter_pistol.o

$(test)/task: $(obj)/promise.o $(obj)/decimal.o $(obj)/event_loop.o $(obj)/starter_pistol.o

$(test)/functional: $(obj)/promise.o $(obj)/event_loop.o $(obj)/starter_pistol.o

$(test)/decimal: $(obj)/decimal.o

$(test)/promise_stream: $(obj)/promise_stream.o $(obj)/promise.o

$(test)/task_stream: $(obj)/promise.o $(obj)/promise_stream.o $(obj)/event_loop.o $(obj)/starter_pistol.o

# Test binaries

$(test)/%: $(obj)/test_%.o $(obj)/assertion.o | $(test)
	@$(define_cc_proxy)
	$(cc) $(ld_opts) $^ -o $@

# Autodependencies

-include $(wildcard $(dep)/*.d)
