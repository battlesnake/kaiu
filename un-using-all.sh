#!/bin/bash

find . '(' -name '*.h' -or -name '*.tcc' ')' -exec perl -i un-using.pl {} \;
