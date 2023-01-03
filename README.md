This code creates an compressed random access memory data structure of given text file.
Contact : wypark2510@gmail.com

**Compressed Random Access Memory**
<br/>
<br/>
Description:<br/>
On darray folder, CRAM is implemented using dynamic array on C programming language.
On btree folder, CRAM is implemented using B+ tree on C++ programming language.

Make:<br/>

For test darray based CRAM, use following commands.
make cram_test
./cram_test "source.txt" "dest.txt" 0 4 0 2

parameters for darray based CRAM
1. source text file
2. dest text file
3. fixed as 0
4. number of replacement(U value on the paper)
5. number of huffman tree type(Type value on the paper)
0 is for CRAM, 1 and 2 is for DCRAM
6. fixed as 2

For test B+tree based CRAM, use following commands,<br/>
<br/>
To compile B+tree based CRAM,<br/>
  g++ -O3 -std=c++20 bench_replace.cpp -o test       (DEFAULT)<br/>
  g++-11 -O3 -std=c++20 bench_replace.cpp -o test    (IF GCC VERSION 11 NEEDED)<br/>
<br/>
To run B+tree based CRAM,<br/>
  ./test "source.txt" "dest.txt"<br/>

CRAM class has template arguments and parameters

example)
CRAM<T,Ch,MODE,H,MAX_BLOCK_SIZE,MAX_INTERNAL_BLOCK_SIZE> cram(source,rewrite_blocks)

T: block storing type, uint64_t fixed
Ch: alphabet character size, uint16_t or uint8_t supported
MODE: 0 is CRAM, 1 and 2 is DCRAM which is on the paper.
H: height of B+ tree, 4 is recommanded
MAX_BLOCK_SIZE: maximum block size 1024 is recommanded
MAX_INTERNAL_BLOCK_SIZE: maximum inner node size 64 is recommanded


**References**<br/>
