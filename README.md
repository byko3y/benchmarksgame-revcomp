
# benchmarksgame-revcomp
Reverse-complement in C for "The Computer Language Benchmarks Game"  
https://benchmarksgame-team.pages.debian.net/benchmarksgame/performance/revcomp.html  
Demonstrates a good performance (at least top 5), while has half the memory usage. Currently it's not much different from other solutions unless some efficient multithreading is implemented.

# Building
Build with *gcc -pipe -Wall -O3 -fomit-frame-pointer -march=native revcomp.c -o revcomp*
