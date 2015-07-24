tests := $(patsubst test_%.cpp, %, $(wildcard test_*.cpp))

mode ?= debug

show_mode_and_goals := $(shell >&2 printf -- "\e[4m%s: [%s]\e[0m\n" "$$(echo $(mode) | tr [:lower:] [:upper:] )" "$(MAKECMDGOALS)")

cc := g++ 2>&1 -fopenmp
#cc := clang++

cc_base := -std=c++14
ld_base := -lpthread

ifeq ($(mode),debug)
cc_opts := $(cc_base) -Wall -Og -g
ld_opts := $(ld_base)
else
ifeq ($(mode),release)
cc_opts := $(cc_base) -flto -w -O3
ld_opts := $(ld_base) -flto
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

default: syntax

syntax:
	$(cc) $(cc_base) -Wall -fsyntax-only $(filter-out test_%, $(wildcard *.cpp)) | c++-color

clean:
	rm -rf -- $(outdirs)

# Run all tests

tests: $(tests:%=test/%)
	@for test in $^; do
		printf -- "Running test: '%s'\n" "$${test}"
		"$${test}" --test-silent-if-perfect
		printf -- "\n"
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
	$(cc) $(cc_opts) $< -MMD -MF $(dep)/$*.d -MQ $@ -c -o $@ | c++-color || rm -f -- $@

# Projects

$(out)/%: $(obj)/%.o | $(out)
	$(cc) $(ld_opts) $^ -o $@ | c++-color || rm -f -- $@

# Test dependencies

$(test)/promise:

$(test)/event_loop: $(obj)/starter_pistol.o

$(test)/task: $(obj)/promise.o $(obj)/decimal.o $(obj)/event_loop.o $(obj)/starter_pistol.o

# Test binaries

$(test)/%: $(obj)/test_%.o $(obj)/%.o $(obj)/assertion.o | $(test)
	$(cc) $(ld_opts) $^ -o $@ | c++-color || rm -f -- $@

# Autodependencies

-include $(wildcard $(dep)/*.d)
