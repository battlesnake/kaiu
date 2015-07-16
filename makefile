tests := $(patsubst test_%.cpp, %, $(wildcard test_*.cpp))

cxxextra ?= -g -O0 -DDEBUG

cxx := g++ -std=c++14
#cxx := clang++ -std=c++14

cxxopts := -lpthread -Wall $(cxxextra)

.PHONY: test clean default all list-deps stats tests

default: all

all:
	$(cxx) $(cxxopts) -fsyntax-only $(wildcard *.cpp) 2>&1 | c++-color

clean:
	rm -rf -- test/

list-deps:
	@perl -ne 'print $$1."\n" if m/#include <([^>]+)>/' *.h *.cpp *.tcc | sort | uniq

test/:
	mkdir -p $@

test/promise: promise.cpp spinlock.cpp

test/task: decimal.cpp promise.cpp spinlock.cpp event_loop.cpp starter_pistol.cpp

test/event_loop: starter_pistol.cpp

test/%: test_%.cpp %.cpp assertion.cpp | test/
	$(cxx) $(cxxopts) $^ -o $@ 2>&1 | c++-color

test: $(tests:%=test/%) | test/
	@echo ""
	@for test in $^; do \
		printf -- "Running test: '%s'\n\n" "$${test}"; \
		"$${test}"; \
		printf -- "\n\n"; \
	done

stats:
	@( \
		printf -- "Lines\tChars\tUnit name\n"; \
		for root in $$(ls *.{cpp,tcc,h} | sed -e 's/\..*$$//g' | sort -u); do \
			printf -- "%s\t%s\t%s\n" $$(cat $${root}.* | wc -cl) "$${root}"; \
		done | sort -rnt$$'\t'; \
	) | column -t -o' | ' -s$$'\t'

tests:
	./run_test run: $(tests)
