tests := trigger event_loop promise

cxxextra ?= -g -O0 -DDEBUG

cxx := g++ -std=c++14
#cxx := clang++ -std=c++14

cxxopts := -lpthread -Wall $(cxxextra)

.PHONY: test clean default all list-deps stats

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

test/task: task.cpp promise.cpp spinlock.cpp event_loop.cpp decimal.cpp

test/%: %.cpp | test/
	$(cxx) $(cxxopts) -D$(<:%.cpp=test_%) $^ -o $@ 2>&1 | c++-color

test: $(tests:%=test/%) | test/
	@echo ""
	@for test in $^; do \
		printf -- "Running test: '%s'\n\n" "$${test}"; \
		"$${test}"; \
		printf -- "\n\n"; \
	done

stats:
	wc -cl *.{cpp,tcc,h} | sort -h
