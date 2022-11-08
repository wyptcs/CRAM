This code creates an compressed random access memory data structure of given text file.
Contact : wypark2510@gmail.com

**data structures for answering RT2Q**
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
6. fixed as 2

For test B+tree based CRAM, use following commands,
g++-11 -O3 -std=c++20 bench_replace.cpp -o test
./test /home/wyp/data/english.txt /home/wyp/data/dna.txt

**References**<br/>
