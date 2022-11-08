#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <fstream>
#include <assert.h>
#include "cram_replace.h"
using namespace std;
using namespace std::chrono;
using code_t = uint64_t;
using data_t = uint16_t;
using freq_t = int;
const int SIGMA = 65536;
const int TEST_NUM = 0;

auto build_replace_data(const auto& dest,const int block_size){
    std::vector<std::vector<data_t>> replace_data;
    for(int i=0;i<dest.size();i+=block_size){        
        std::vector<data_t> block(block_size);
        if(TEST_NUM==0){
            copy(dest.begin()+i,dest.begin()+(i+block_size),block.begin());
        }else if(TEST_NUM==1){
            std::transform(block.begin(),block.end(),block.begin(),[](int val){return rand()%SIGMA;});
        }else if(TEST_NUM==2){
            std::fill(block.begin(),block.end(),dest[0]);
        }        
        replace_data.push_back(std::move(block));
    }
    return replace_data;    
}
auto build_rebuild_sequence(const int block_num){
    std::vector<int> sequence(block_num);
    if(TEST_NUM==0){
        std::iota(sequence.begin(),sequence.end(),0);
    }else{
        std::transform(sequence.begin(),sequence.end(),sequence.begin(),[block_num](int val){return rand()%block_num;});
    }
    return sequence;
}

template<typename T,typename Ch,int MODE,int H,int MAX_BLOCK_SIZE = 1024,int MAX_INTERNAL_BLOCK_SIZE = 1024>
auto cram_replace_test(const auto& source,const auto& dest,const int rewrite_blocks){
    assert(source.size()%(MAX_BLOCK_SIZE/2) == 0);
    cout<<"CRAM BUILD START\n";
    CRAM<T,Ch,MODE,H,MAX_BLOCK_SIZE,MAX_INTERNAL_BLOCK_SIZE> cram(source,rewrite_blocks);
    std::vector<double> entropy_vec;

    double ent = cram.get_entropy();
    entropy_vec.push_back(ent);
    cout<<"CRAM BUILD END\n";
    auto replace_data = build_replace_data(dest,MAX_BLOCK_SIZE/2);    
    const int block_num = replace_data.size();    
    auto sequence = build_rebuild_sequence(block_num);
    
    std::vector<double> ret;

    
    auto bpc = cram.get_bpc();
    long long elapsed_time = 0;
    cout<<"PROGRESS: "<<0<<" CRAM BPC: "<<bpc<<" , ENTROPY: "<<cram.get_entropy()<<'\n';
    ret.push_back(bpc);
    for(int i=0;i<block_num;++i){        
        const int block_idx = sequence[i];
        auto start = steady_clock::now();
        cram.replace(block_idx,replace_data[i]);
        auto end = steady_clock::now();
        elapsed_time += duration_cast<nanoseconds> (end - start).count();
        if((i+1)%(block_num/10)==0){
            auto bpc = cram.get_bpc();
            cout<<"PROGRESS: "<<(i+1)/(block_num/10)*10<<" CRAM BPC: "<<bpc<<" , ENTROPY: "<<cram.get_entropy()<<'\n';            
            ret.push_back(bpc);
            double ent = cram.get_entropy();
            entropy_vec.push_back(ent);
        }
    }
    
    ret.insert(ret.end(),entropy_vec.begin(),entropy_vec.end());
    const double elapsed_total_sec = static_cast<double>(elapsed_time)/1000000000;
    const double elapsed_per_operation = static_cast<double>(elapsed_time) / source.size();
    ret.push_back(elapsed_total_sec);
    ret.push_back(elapsed_per_operation);
    ret.push_back(cram.get_rebuild_cnt());
    cout << "CRAM REPLACE Time difference = " << elapsed_total_sec << " [s]\n";
    cout << "CRAM REPLACE PER OPERATION: " << elapsed_per_operation<< " [ns]\n";
    return ret;
}

void push_csv(int mode,int u,const std::vector<double>& vec,ofstream& out){
    out<<mode<<','<<u<<',';
    for(auto val:vec) out<<val<<',';
    out<<'\n';
}

int main(int argc,char **argv){
    std::ifstream is_source(argv[1],std::ios::binary);
    is_source.seekg(0, std::ios_base::end);
    std::size_t size=is_source.tellg();
    is_source.seekg(0, std::ios_base::beg);
    std::vector<data_t> source(size/sizeof(data_t));
    is_source.read((char*) &source[0], size);
    is_source.close();

    std::ifstream is_dest(argv[2],std::ios::binary);
    is_dest.seekg(0, std::ios_base::end);
    size=is_dest.tellg();
    is_dest.seekg(0, std::ios_base::beg);
    std::vector<data_t> dest(size/sizeof(data_t));
    is_dest.read((char*) &dest[0], size);
    is_dest.close();

    std::ofstream out("test.csv");
    cout<<"CRAM REPLACE TEST\n";
    const std::vector<int> rewrite_blocks_vec_sada = {4,2,1};    
    const std::vector<int> rewrite_blocks_vec_icalp = {4,2,1,0};

    for(int rewrite_blocks:rewrite_blocks_vec_sada){
        auto ret = cram_replace_test<code_t,data_t,0,4,1024,64>(source,dest,rewrite_blocks);
        push_csv(0,rewrite_blocks,ret,out);
    }
    for(int rewrite_blocks:rewrite_blocks_vec_icalp){
        auto ret = cram_replace_test<code_t,data_t,1,4,1024,64>(source,dest,rewrite_blocks);
        push_csv(1,rewrite_blocks,ret,out);
    }
    for(int rewrite_blocks:rewrite_blocks_vec_icalp){
        auto ret = cram_replace_test<code_t,data_t,2,4,1024,64>(source,dest,rewrite_blocks);
        push_csv(2,rewrite_blocks,ret,out);
    }
}