tests := trigger event_loop promise

#cxx := g++ -std=c++14
cxx := clang++ -std=c++14
cxxopts := -lpthread -Wall -g -O0

.PHONY: test clean default all list-deps

default: all

all:
	$(cxx) $(cxxopts) -fsyntax-only $(wildcard *.cpp) 2>&1 | pretty-log

clean:
	rm -rf -- test/

list-deps:
	@perl -ne 'print $$1."\n" if m/#include <([^>]+)>/' *.h *.cpp *.tpp | sort | uniq

test/:
	mkdir -p $@

test/promise: promise.cpp spinlock.cpp | test/
	$(cxx) $(cxxopts) -D$(<:%.cpp=test_%) $^ -o $@ 2>&1 | pretty-log

test/%: %.cpp | test/
	$(cxx) $(cxxopts) -D$(<:%.cpp=test_%) $^ -o $@ 2>&1 | pretty-log

test: $(tests:%=test/%) | test/
	@echo ""
	@for test in $^; do \
		printf -- "Running test: '%s'\n\n" "$${test}"; \
		"$${test}"; \
		printf -- "\n\n"; \
	done
