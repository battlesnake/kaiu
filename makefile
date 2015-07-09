tests := trigger event_loop

cxx := g++
#cxx := clang++

.PHONY: test clean default all list-deps

default: all

all:
	$(cxx) -std=c++14 $(wildcard *.cpp) -lpthread -Wall -g -fsyntax-only 2>&1 | pretty-log

clean:
	rm -rf -- test/

list-deps:
	@perl -ne 'print $$1."\n" if m/#include <([^>]+)>/' *.h *.cpp *.tpp | sort | uniq

test/:
	mkdir -p $@

test/%: %.cpp | test/
	$(cxx) -std=c++14 $^ -D$(<:%.cpp=test_%) -lpthread -Wall -g -o $@ 2>&1 | pretty-log

test: $(tests:%=test/%) | test/
	@echo ""
	@for test in $^; do \
		printf -- "Running test: '%s'\n\n" "$${test}"; \
		"$${test}"; \
		printf -- "\n\n"; \
	done
