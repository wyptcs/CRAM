#ifndef CRAM_INSERT_H
#define CRAM_INSERT_H

#include "general_darray.h"
#include "block.h"
using std::cout;
using namespace std::chrono;

template<typename T,typename Ch,int MODE,int H,int MAX_BLOCK_SIZE = 1024,int MAX_INTERNAL_BLOCK_SIZE = 1024,int SIGMA = 65536>
class CRAM{
public:
    using index_t = int;
    //using block_t = HuffmanBlock<T,Ch,MAX_BLOCK_SIZE>;
    using block_t = SpecialBlock<T,MAX_BLOCK_SIZE>;
    using encoder_t = HuffmanEncoder<T,Ch,MODE>;
    CRAM() = delete;    
    explicit CRAM(const auto& text,const int rewrite_blocks_) : 
    freq(SIGMA,1),curr_tree(0),prev_tree(0),modify_cnt(0),block_size(MAX_BLOCK_SIZE/2),existCodeSpace(true),bulk_rewrite_cnt(0),
    da_time(0){                
        total_block_num = text.size()/block_size;
        rewrite_blocks = rewrite_blocks_;
        rewrite_start_pos = rewrite_blocks==0 ? 0 : (text.size())*(rewrite_blocks-1)/rewrite_blocks;        
        for(const auto ch:text){
            freq[ch]++;
        }
        encoders[0] = encoder_t(freq);
        if constexpr(0 < MODE){
            pivotFreq.resize(SIGMA);
            std::copy(freq.begin(),freq.end(),pivotFreq.begin());
        }        
        da = Darray<T,Ch,block_t,encoder_t,H,MAX_BLOCK_SIZE,MAX_INTERNAL_BLOCK_SIZE>(text,encoders[0]);        
    }    
    
    void makeNewCode(Ch ch){
        if(!existCodeSpace) return;
        int candidate_len = std::max(3.0,std::log2(da.size()/freq[ch]));
        if(pivotFreq[ch]*2 < freq[ch] && candidate_len < 16){
            existCodeSpace = encoders[curr_tree].insertCode(ch,candidate_len);
            pivotFreq[ch] = freq[ch];
        }
    }
    void insert_bulkrewrite(index_t pos,Ch ch){                
        if(!existCodeSpace){
            ++bulk_rewrite_cnt;
            std::cout<<"BULK REBUILD\n";
            curr_tree^=1;            
            encoders[curr_tree] = encoder_t(freq);            
            index_t pos = 0;            
            int i = 0;
            while(pos < da.size()){
                const auto curr_block = da.block_at(pos,encoders);
                da.block_replace(pos,curr_block,encoders,curr_tree);
                pos+=curr_block.size();                
            }            
            prev_tree = curr_tree;
            pivotFreq = freq;
            existCodeSpace = true;
            std::cout<<"BULK REBUILD FIN"<<std::endl;
        }        
        da.insert(pos,ch,encoders);        
    }
    void erase(index_t pos){
        auto erase_start = steady_clock::now();
        auto erased_ch = da.erase(pos,encoders);
        auto erase_end = steady_clock::now();
        da_time+=duration_cast<nanoseconds> (erase_end - erase_start).count();
        ++modify_cnt;
        if(pos < rewrite_start_pos) --rewrite_start_pos;
        //freq[erased_ch]--;
        if(MAX_BLOCK_SIZE/2 == modify_cnt){            
            modify_cnt = 0;
            for(int i=0;i<rewrite_blocks && rewrite_start_pos < da.size();++i){
                const auto curr_block = da.block_at(rewrite_start_pos,encoders);
                da.block_replace(rewrite_start_pos,curr_block,encoders,curr_tree);
                rewrite_start_pos+=curr_block.size();
            }            
            if(rewrite_start_pos==da.size()){
                //std::cout<<"REWRITE! "<<prev_tree<<" , "<<curr_tree<<'\n';                
                rewrite_start_pos = 0;
                if(MODE==0 || (!existCodeSpace && rewrite_blocks > 0)){
                    ++bulk_rewrite_cnt;
                    prev_tree = curr_tree;
                    curr_tree^=1;
                    encoders[curr_tree] = encoder_t(freq);              
                    if constexpr(0 < MODE){
                        pivotFreq = freq;
                        existCodeSpace = true;
                    }
                }                
            }            
        }
    }
    void insert(index_t pos,Ch ch){
        freq[ch]++;
        if constexpr(0 < MODE){
            makeNewCode(ch);
        }
        if(rewrite_blocks==0){
            insert_bulkrewrite(pos,ch);
            return;
        }

        //uint8_t tree_idx = pos < rewrite_start_pos ? curr_tree : prev_tree;
        
        auto insert_start = steady_clock::now();
        da.insert(pos,ch,encoders);        
        auto insert_end = steady_clock::now();
        da_time+=duration_cast<nanoseconds> (insert_end - insert_start).count();

        ++modify_cnt;
        if(pos < rewrite_start_pos) ++rewrite_start_pos;
        if(MAX_BLOCK_SIZE/2 == modify_cnt){            
            modify_cnt = 0;
            for(int i=0;i<rewrite_blocks && rewrite_start_pos < da.size();++i){                
                const auto curr_block = da.block_at(rewrite_start_pos,encoders);
                da.block_replace(rewrite_start_pos,curr_block,encoders,curr_tree);
                rewrite_start_pos+=curr_block.size();
            }            
            if(rewrite_start_pos==da.size()){
                //std::cout<<"REWRITE! "<<prev_tree<<" , "<<curr_tree<<'\n';                
                rewrite_start_pos = 0;                
                if(MODE==0 || !existCodeSpace){
                    ++bulk_rewrite_cnt;
                    prev_tree = curr_tree;
                    curr_tree^=1;
                    encoders[curr_tree] = encoder_t(freq);              
                    if constexpr(0 < MODE){
                        pivotFreq = freq;
                        existCodeSpace = true;
                    }
                }                
            }            
        }
    }
    auto get_block(index_t pos){
        int tree_idx = pos < rewrite_start_pos ? curr_tree : prev_tree;
        return da.block_at(pos,encoders[tree_idx]);
    }
    auto get_bpc()const{
        return da.get_bpc();
    }
    auto get_entropy() const{
        const int n = da.size()+SIGMA;
        return std::transform_reduce(freq.begin(),freq.end(),static_cast<double>(0),std::plus{},[n](const auto f){
            const auto p = static_cast<double>(f)/n;
            return -std::log2(p)*p;
        })/2;
    }
    auto size() const{
        return da.size();
    }    
    auto get_bulkcnt() const{
        return bulk_rewrite_cnt;
    }
    auto get_datime() const{
        return da_time;
    }


private:
    index_t total_block_num;
    index_t rewrite_blocks;    
    index_t block_size;

    int prev_tree;
    int curr_tree;
    int bulk_rewrite_cnt;

    index_t rewrite_start_pos;    
    index_t modify_cnt;
    
    bool existCodeSpace;   

    long long da_time;

    std::vector<int> freq;
    std::vector<int> pivotFreq;    
    std::array<encoder_t,2> encoders;    
    Darray<T,Ch,block_t,encoder_t,H,MAX_BLOCK_SIZE,MAX_INTERNAL_BLOCK_SIZE> da;
};

#endif