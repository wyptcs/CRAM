#ifndef BLOCK_H_
#define BLOCK_H_
#include <iostream>
#include <vector>
#include <cstring>
#include <assert.h>
#include <memory>
#include <array>
#include <execution>
#include "encoder.h"

const bool DEBUG = true;

template<typename T,typename Ch>
class NaiveBlock{
private:
    size_t block_size;
    std::vector<T> block_vec;
    std::vector<T>& get(){
        return std::ref(block_vec);
    }
public:    
    NaiveBlock() : block_size(0){}
    explicit NaiveBlock(int size) : block_size(size),block_vec(size){};
    explicit NaiveBlock(std::vector<T>& block) : block_size(block.size()),block_vec(block){}
    explicit NaiveBlock(std::vector<T>&& block) : block_size(block.size()),block_vec(block){}
    NaiveBlock(const NaiveBlock<T,Ch>&) = default;
    NaiveBlock& operator=(const NaiveBlock<T,Ch>&) = default;
    NaiveBlock(NaiveBlock<T,Ch>&&) noexcept = default;
    NaiveBlock& operator=(NaiveBlock<T,Ch>&&) noexcept = default;
    
    
    T at(size_t pos,const auto& encoder) const {
        if constexpr(DEBUG){
            assert(pos>=0 && pos<block_size);
        }
        return block_vec[pos];
    }
    size_t size() const {
        if constexpr(DEBUG) {block_size==block_vec.size();}
        return block_size;
    }

    void replace(size_t pos,const Ch& ch,const auto& encoder){
        if constexpr(DEBUG) assert(pos < block_size);
        block_vec[pos] = ch;
    }
    void insert(size_t pos,const Ch& ch,const auto& encoder){
        /*insert before pos
        block = [2,3,4,5,6]
        block.insert(0,11) => [11,2,3,4,5,6]*/
        if constexpr(DEBUG) assert(pos <= block_size);
        block_vec.insert(block_vec.begin()+pos,ch);
        ++block_size;
    }
    void erase(size_t pos,const auto& encoder){
        /*erase curr pos
        block = [2,3,4,5,6]
        block.erase(2) => [2,4,5,6]*/
        if constexpr(DEBUG) assert(pos < block_size);
        block_vec.erase(block_vec.begin()+pos);
        --block_size;
    }
    /*void merge(NaiveBlock<T,Ch>& rhs,const auto& left_encoder,const auto& right_encoder){
        merge two blocks to curr
        curr = [1,2,3,4] rhs = [11,22,33,44]
        curr.merge(rhs) => [1,2,3,4,11,22,33,44]
        block_vec.insert(block_vec.end(),rhs.get().begin(),rhs.get().end());
        block_size += rhs.block_vec.size();
    }*/
    void merge(auto rhs_unique_ptr,const auto& left_encoder,const auto& right_encoder){                
        block_vec.insert(block_vec.end(),rhs_unique_ptr->get().cbegin(),rhs_unique_ptr->get().cend());
        block_size += rhs_unique_ptr->size();
    }
    auto split(size_t pos,const auto& encoder){
        /*split curr into two blocks
        block = [1,2,3,4,5]
        block.split(2) => [1,2] [3,4,5]*/
        if constexpr(DEBUG) assert(pos>0 && pos<block_size);
        size_t lsize = pos, rsize = block_size - pos;
        std::vector<T> l_vec(lsize);
        std::vector<T> r_vec(rsize);
        std::copy(block_vec.cbegin(),block_vec.cbegin()+lsize,l_vec.begin());
        std::copy(block_vec.cbegin()+lsize,block_vec.cend(),r_vec.begin());
        NaiveBlock<T,Ch> l_block(std::move(l_vec));
        NaiveBlock<T,Ch> r_block(std::move(r_vec));
        return make_pair<NaiveBlock<T,Ch>,NaiveBlock<T,Ch>>(std::move(l_block),std::move(r_block));
    }
};

template<typename T,typename Ch>
auto encode_block(const auto& block,const auto& encoder){
    std::vector<T> ret;        
    int total_size = std::transform_reduce(block.begin(),block.end(),0,std::plus{},
    [&encoder](const auto& ch){return encoder.encode(ch).second;});        
    int comp_pos = 0,encoded_blocks = 0;      
    const int block_bits = sizeof(T)*8;          
    ret.resize((total_size+block_bits-1)/block_bits,static_cast<T>(0));
    const int n = ret.size();
    for(auto ch:block){
        auto [code,len] = encoder.encode(ch);                   
        auto block_index = comp_pos/block_bits;
        auto block_offset = comp_pos%block_bits;
        auto remain = block_offset+static_cast<int>(len)-block_bits;
        ret[block_index] |= (code>>block_offset);
        if(block_index+1 < n)
            ret[block_index+1] |= remain > 0 ? (code<<(static_cast<int>(len)-remain)) : static_cast<T>(0);
        comp_pos+=len;
    }
    return std::make_pair(ret,total_size);
}
template<typename T,typename Ch>
auto decode_block(const auto& comp_block,const int block_size,const auto& encoder){
    std::vector<Ch> ret;        
    if(block_size==0) return ret;
    ret.resize(block_size);
    auto comp_block_iter = comp_block.begin();int comp_pos = 0;
    const int block_bits = sizeof(T)*8;
    T curr_code = *comp_block_iter,next_code = (next(comp_block_iter) == comp_block.end() ? 0 : *std::next(comp_block_iter));
    for(int i=0;i<block_size;++i){
        auto block_index = comp_pos/block_bits;
        auto block_offset = comp_pos%block_bits;            
        auto code = curr_code<<block_offset;        
        code |= (next(comp_block_iter) == comp_block.end() || block_offset==0 ? 0 : next_code>>(block_bits - block_offset));
        auto [ch,len] = encoder.decode(code);
        comp_pos += len;
        std::advance(comp_block_iter,comp_pos/block_bits==block_index ? 0 : 1);
        curr_code = comp_pos/block_bits==block_index ? curr_code : next_code;
        next_code = comp_pos/block_bits==block_index ? next_code : (next(comp_block_iter) == comp_block.end() ? 0 : *next(comp_block_iter));
        ret[i] = ch;            
    }    
    return ret;
}

/*
template<typename T,typename Ch,int MAX_BLOCK_SIZE,int U = 4>//U: superchar num == 4
class HuffmanBlock{
private:
    size_t block_size;    
    size_t total_block_bits;
    std::vector<T> block_vec;    
    inline auto get_requried_blocks(int total_bits) const {        
        return (total_bits+sizeof(T)*8-1)/(sizeof(T)*8);
    }
public:    
    HuffmanBlock() = default;    
    
    explicit HuffmanBlock(int size) : block_size(size),block_vec(size),total_block_bits(size){}
    explicit HuffmanBlock(const auto& block,const auto& encoder){
        block_size = block.size();
        auto [vec,len] = encode_block<T,Ch>(block,encoder);
        block_vec = std::move(vec);
        total_block_bits = len;
    }    
    auto get_start_shift_pos(const int start_shift_idx,const auto& encoder) const{
        if(start_shift_idx==0) return 0;
        assert(!block_vec.empty());
        auto block_vec_iter = block_vec.begin();int comp_pos = 0;
        const int block_bits = sizeof(T)*8;        
        T curr_code = *block_vec_iter,next_code = (std::next(block_vec_iter) == block_vec.end() ? 0 : *std::next(block_vec_iter));
        for(int i=0;i<start_shift_idx;++i){
            const auto block_index = comp_pos/block_bits;
            const auto block_offset = comp_pos%block_bits;            
            auto code = curr_code<<block_offset;        
            code |= (next(block_vec_iter) == block_vec.end() || block_offset==0 ? 0 : next_code>>(block_bits - block_offset));
            auto [ch,len] = encoder.decode(code);
            comp_pos+=len;
            std::advance(block_vec_iter,comp_pos/block_bits==block_index ? 0 : 1);
            curr_code = comp_pos/block_bits==block_index ? curr_code : next_code;
            next_code = comp_pos/block_bits==block_index ? next_code : (std::next(block_vec_iter) == block_vec.end() ? 0 : *std::next(block_vec_iter));
        }
        return comp_pos;
    }
    void shift_left(const int start_shift_pos,const int shift_len){
        std::array<T,MAX_BLOCK_SIZE/2> temp_code_vec = {0};        
        const bool pop_block = get_requried_blocks(total_block_bits) != get_requried_blocks(total_block_bits-shift_len);
        constexpr int code_size = sizeof(T)*8;
        const int start_idx = (start_shift_pos - shift_len)/code_size;
        const int start_shift_pos_minus_one_inblock = (start_shift_pos - shift_len)%code_size;        
        const int block_vec_size = block_vec.size();
        for(int i=start_idx;i<block_vec_size;++i){
            temp_code_vec[i-start_idx] = block_vec[i]<<shift_len;
            temp_code_vec[i-start_idx] |= (i==block_vec_size-1 ? static_cast<T>(0) : block_vec[i+1])>>(code_size - shift_len);
        }        
        if(start_shift_pos_minus_one_inblock > 0){
            T white = static_cast<T>(-1)>>start_shift_pos_minus_one_inblock;
            temp_code_vec[0]&=white;
            T left_code = block_vec[start_idx]&(~white);
            temp_code_vec[0]|=left_code;
        }
        std::copy(temp_code_vec.begin(),temp_code_vec.begin()+(block_vec_size-start_idx),block_vec.begin()+start_idx);
        if(pop_block) block_vec.pop_back();
        total_block_bits -= shift_len;        
    }
    void shift_right(const int start_shift_pos,const int shift_len){
        std::array<T,MAX_BLOCK_SIZE/2> temp_code_vec = {0};        
        const bool add_block = get_requried_blocks(total_block_bits) != get_requried_blocks(total_block_bits+shift_len);
        constexpr int code_size = sizeof(T)*8;     
        const int start_idx = start_shift_pos/code_size;   
        if(add_block) block_vec.push_back(static_cast<T>(0));
        const int block_vec_size = block_vec.size();
        for(int i=start_idx;i<block_vec_size;++i){
            temp_code_vec[i-start_idx] = block_vec[i]>>shift_len;
            temp_code_vec[i-start_idx] |= (i==0 ? static_cast<T>(0) : block_vec[i-1])<<(code_size - shift_len);
        }
        const int start_shift_pos_inblock = start_shift_pos%code_size;
        if(start_shift_pos_inblock+shift_len <= code_size){           
            const int valid_bit_size = code_size - start_shift_pos_inblock - shift_len;
            const int white_size = code_size - start_shift_pos_inblock;
            T white_mask = (static_cast<T>(-1))>>start_shift_pos_inblock;                 
            T shift_mask = (static_cast<T>(1)<<valid_bit_size)-1;            
            T right_remain = ((block_vec[start_idx]>>shift_len)&shift_mask);
            temp_code_vec[0] = block_vec[start_idx];
            temp_code_vec[0] &= (~white_mask);
            temp_code_vec[0] |= right_remain;
        }else{
            assert(start_idx+1 < block_vec.size());
            const int left_white_size = code_size - start_shift_pos_inblock;
            const int right_white_size = start_shift_pos_inblock+shift_len-code_size;
            T left_white_mask = (static_cast<T>(1)<<left_white_size)-1;
            T right_white_mask = static_cast<T>(-1)<<(code_size - right_white_size);  
            temp_code_vec[0] = block_vec[start_idx];
            temp_code_vec[0] &= (~left_white_mask);
            temp_code_vec[1] &= (~right_white_mask);
        }
        std::copy(temp_code_vec.begin(),temp_code_vec.begin()+(block_vec_size - start_idx),block_vec.begin()+start_idx);
        total_block_bits += shift_len;
    }   
    void insert(size_t pos,const Ch& ch,const auto& encoder){        
        if constexpr(DEBUG) assert(pos <= block_size);        
        constexpr int code_size = sizeof(T)*8;     
        const int start_shift_pos = get_start_shift_pos(pos,encoder);
        auto [code,len] = encoder.encode(ch);              
        shift_right(start_shift_pos,len);        
        const int start_shift_pos_inblock = start_shift_pos%code_size;
        const int start_idx = start_shift_pos/code_size;        
        if(start_shift_pos_inblock+len <= code_size){            
            code>>=start_shift_pos_inblock;
            block_vec[start_idx] |= code;
        }else{
            auto left_code = code>>start_shift_pos_inblock;
            auto right_code = code<<(code_size - start_shift_pos_inblock);
            block_vec[start_idx] |= left_code;
            block_vec[start_idx+1] |= right_code;
        }
        ++block_size;        
        const int lastblock_valid_bits = (total_block_bits%code_size == 0 ? code_size : total_block_bits%code_size);
        T white = static_cast<T>(-1)<<(code_size - lastblock_valid_bits);
        block_vec.back() &= white;
    }

    void erase(size_t pos,const auto& encoder){
        if constexpr(DEBUG) assert(pos < block_size);        
        constexpr int code_size = sizeof(T)*8;     
        const int start_shift_pos_minus_one = get_start_shift_pos(pos,encoder);
        const int start_idx = start_shift_pos_minus_one/code_size;
        const int left_bits = start_shift_pos_minus_one%code_size;
        const int curr_block_size = block_vec.size();        
        const T code = ((block_vec[start_idx]<<left_bits) | ((left_bits==0 || start_idx==curr_block_size-1) ? static_cast<T>(0) : block_vec[start_idx+1]>>(code_size - left_bits)));
        const int erasecode_len = encoder.decode(code).second;
        //cout<<"erasecode len: "<<erasecode_len<<" ,left bits: "<<left_bits<<'\n';
        shift_left(start_shift_pos_minus_one+erasecode_len,erasecode_len);
        --block_size;
        const int lastblock_valid_bits = (total_block_bits%code_size == 0 ? code_size : total_block_bits%code_size);
        T white = static_cast<T>(-1)<<(code_size - lastblock_valid_bits);
        block_vec.back() &= white;
    }
    void replace(size_t pos,const Ch& ch,const auto& encoder){
        if constexpr(DEBUG) assert(0 <= pos && pos < block_size);        
        const int code_size = sizeof(T)*8; 
        const int start_shift_pos_minus_one = get_start_shift_pos(pos,encoder);
        const int start_idx = start_shift_pos_minus_one/code_size;
        const int left_bits = start_shift_pos_minus_one%code_size;
        const int curr_block_size = block_vec.size();        
        const T code = ((block_vec[start_idx]<<left_bits) | ((left_bits==0 || start_idx==curr_block_size-1) ? static_cast<T>(0) : block_vec[start_idx+1]>>(code_size - left_bits)));
        const int prevcode_len = encoder.decode(code).second;
        auto [currcode,currcode_len] = encoder.encode(ch);        
        //std::cout<<total_block_bits<<','<<start_shift_pos_minus_one<<','<<static_cast<int>(prevcode_len)<<','<<static_cast<int>(currcode_len)<<std::endl;
        if(left_bits + prevcode_len <= code_size){
            T white = ((static_cast<T>(1)<<prevcode_len)-1)<<(code_size - left_bits - prevcode_len);
            block_vec[start_idx] &= (~white);            
        }else{
            T left_white = static_cast<T>(-1)>>left_bits;
            T right_white = static_cast<T>(-1)>>(left_bits + prevcode_len - code_size);
            block_vec[start_idx] &= (~left_white);
            block_vec[start_idx+1] &= right_white;
        }        
        if(currcode_len < prevcode_len) shift_left(start_shift_pos_minus_one+prevcode_len,prevcode_len - currcode_len);
        else if(prevcode_len < currcode_len) shift_right(start_shift_pos_minus_one+prevcode_len,currcode_len - prevcode_len);        
        if(left_bits + currcode_len <= code_size){
            currcode>>=left_bits;
            block_vec[start_idx] |= currcode;
        }else{
            auto left_code = currcode>>left_bits;
            auto right_code = currcode<<(code_size - left_bits);
            block_vec[start_idx] |= left_code;
            block_vec[start_idx+1] |= right_code;            
        }
        const int lastblock_valid_bits = (total_block_bits%code_size == 0 ? code_size : total_block_bits%code_size);
        T white = static_cast<T>(-1)<<(code_size - lastblock_valid_bits);
        block_vec.back() &= white;
    }
    std::vector<Ch> get(const auto& encoder) const {
        return decode_block<T,Ch>(block_vec,block_size,encoder);
    }
    Ch at(size_t pos,const auto& encoder) const {
        if constexpr(DEBUG){
            assert(pos>=0 && pos<block_size);
        }
        const int code_size = sizeof(T)*8; 
        const int start_shift_pos_minus_one = get_start_shift_pos(pos,encoder);
        const int start_idx = start_shift_pos_minus_one/code_size;
        const int left_bits = start_shift_pos_minus_one%code_size;
        const int curr_block_size = block_vec.size();        
        const T code = ((block_vec[start_idx]<<left_bits) | ((left_bits==0 || start_idx==curr_block_size-1) ? static_cast<T>(0) : block_vec[start_idx+1]>>(code_size - left_bits)));
        auto [ch,len] = encoder.decode(code);
        return ch;
    }
    size_t use_bytes() const{
        return block_vec.size()*sizeof(T);
    }
    size_t size() const {        
        return block_size;
    }
    void replace_block(const auto& block,const auto& encoder){
        block_size = block.size();
        auto [vec,len] = encode_block<T,Ch>(block,encoder);
        block_vec = std::move(vec);
        total_block_bits = len;
    }

    void merge(auto rhs_unique_ptr,const auto& left_encoder,const auto& right_encoder){
        auto decoded_left = decode_block<T,Ch>(block_vec,block_size,left_encoder);
        auto decoded_right = rhs_unique_ptr->get(right_encoder);        
        block_size+=decoded_right.size();        
        decoded_left.insert(decoded_left.end(),decoded_right.begin(),decoded_right.end());
        auto [vec,len] = encode_block<T,Ch>(decoded_left,left_encoder);
        block_vec = std::move(vec);
        total_block_bits = len;
    }
    auto split(size_t pos,const auto& encoder){        
        if constexpr(DEBUG) assert(pos>0 && pos<block_size);
        size_t lsize = pos, rsize = block_size - pos;
        auto decoded = decode_block<T,Ch>(block_vec,block_size,encoder);
        std::vector<Ch> l_vec(lsize);
        std::vector<Ch> r_vec(rsize);
        std::copy(decoded.begin(),decoded.begin()+lsize,l_vec.begin());
        std::copy(decoded.begin()+lsize,decoded.end(),r_vec.begin());
        HuffmanBlock<T,Ch,MAX_BLOCK_SIZE> l_block(l_vec,encoder);
        HuffmanBlock<T,Ch,MAX_BLOCK_SIZE> r_block(r_vec,encoder);
        return std::make_pair(std::move(l_block),std::move(r_block));
    }
};*/

template<typename T,typename Ch,int MAX_BLOCK_SIZE,int U = 4>//U: superchar num == 4
class HuffmanBlock{
//private:
public:    
    size_t block_size;    
    size_t total_block_bits;
    std::vector<T> block_vec;    
    std::vector<int> cached_pos_vec;
    inline auto get_requried_blocks(int total_bits) const {        
        return (total_bits+sizeof(T)*8-1)/(sizeof(T)*8);
    }
    void validate_cache(const auto& encoder){
        cached_pos_vec.resize(block_size+1);
        std::fill(cached_pos_vec.begin(),cached_pos_vec.end(),0);
        if(block_size==0) return;
        assert(!block_vec.empty());
        auto block_vec_iter = block_vec.begin();int comp_pos = 0;
        const int block_bits = sizeof(T)*8;        
        T curr_code = *block_vec_iter,next_code = (std::next(block_vec_iter) == block_vec.end() ? 0 : *std::next(block_vec_iter));
        for(int i=0;i<block_size;++i){
            const auto block_index = comp_pos/block_bits;
            const auto block_offset = comp_pos%block_bits;            
            auto code = curr_code<<block_offset;        
            code |= (next(block_vec_iter) == block_vec.end() || block_offset==0 ? 0 : next_code>>(block_bits - block_offset));            
            auto [ch,len] = encoder.decode(code);
            cached_pos_vec[i+1] = len;
            comp_pos+=len;
            std::advance(block_vec_iter,comp_pos/block_bits==block_index ? 0 : 1);
            curr_code = comp_pos/block_bits==block_index ? curr_code : next_code;
            next_code = comp_pos/block_bits==block_index ? next_code : (std::next(block_vec_iter) == block_vec.end() ? 0 : *std::next(block_vec_iter));
        }
        std::inclusive_scan(cached_pos_vec.begin(),cached_pos_vec.end(),cached_pos_vec.begin());
    }
    void invalidate_cache(){
        cached_pos_vec.clear();
    }
    inline bool is_valid_cache(){
        return !cached_pos_vec.empty();
    }
public:    
    HuffmanBlock() = default;    
    
    //explicit HuffmanBlock(int size) : block_size(size),block_vec(size),cached_pos_vec(size+1),total_block_bits(size){}
    explicit HuffmanBlock(int size){        
        block_size = 0;
        total_block_bits = 0;        
    }    
    explicit HuffmanBlock(const auto& block,const auto& encoder){
        block_size = block.size();
        auto [vec,len] = encode_block<T,Ch>(block,encoder);
        block_vec = std::move(vec);
        total_block_bits = len;
    }    
    auto get_start_shift_pos(const int start_shift_idx,const auto& encoder) const{
        if(start_shift_idx==0) return 0;
        assert(!block_vec.empty());
        auto block_vec_iter = block_vec.begin();int comp_pos = 0;
        const int block_bits = sizeof(T)*8;        
        T curr_code = *block_vec_iter,next_code = (std::next(block_vec_iter) == block_vec.end() ? 0 : *std::next(block_vec_iter));
        for(int i=0;i<start_shift_idx;++i){
            const auto block_index = comp_pos/block_bits;
            const auto block_offset = comp_pos%block_bits;            
            auto code = curr_code<<block_offset;        
            code |= (next(block_vec_iter) == block_vec.end() || block_offset==0 ? 0 : next_code>>(block_bits - block_offset));
            auto [ch,len] = encoder.decode(code);
            comp_pos+=len;
            std::advance(block_vec_iter,comp_pos/block_bits==block_index ? 0 : 1);
            curr_code = comp_pos/block_bits==block_index ? curr_code : next_code;
            next_code = comp_pos/block_bits==block_index ? next_code : (std::next(block_vec_iter) == block_vec.end() ? 0 : *std::next(block_vec_iter));
        }
        return comp_pos;
    }
    void shift_left(const int start_shift_pos,const int shift_len){
        std::array<T,MAX_BLOCK_SIZE/2> temp_code_vec = {0};        
        const bool pop_block = get_requried_blocks(total_block_bits) != get_requried_blocks(total_block_bits-shift_len);
        constexpr int code_size = sizeof(T)*8;
        const int start_idx = (start_shift_pos - shift_len)/code_size;
        const int start_shift_pos_minus_one_inblock = (start_shift_pos - shift_len)%code_size;        
        const int block_vec_size = block_vec.size();
        for(int i=start_idx;i<block_vec_size;++i){
            temp_code_vec[i-start_idx] = block_vec[i]<<shift_len;
            temp_code_vec[i-start_idx] |= (i==block_vec_size-1 ? static_cast<T>(0) : block_vec[i+1])>>(code_size - shift_len);
        }        
        if(start_shift_pos_minus_one_inblock > 0){
            T white = static_cast<T>(-1)>>start_shift_pos_minus_one_inblock;
            temp_code_vec[0]&=white;
            T left_code = block_vec[start_idx]&(~white);
            temp_code_vec[0]|=left_code;
        }
        std::copy(temp_code_vec.begin(),temp_code_vec.begin()+(block_vec_size-start_idx),block_vec.begin()+start_idx);
        if(pop_block) block_vec.pop_back();
        total_block_bits -= shift_len;        
    }
    void shift_right(const int start_shift_pos,const int shift_len){
        std::array<T,MAX_BLOCK_SIZE/2> temp_code_vec = {0};        
        const bool add_block = get_requried_blocks(total_block_bits) != get_requried_blocks(total_block_bits+shift_len);
        constexpr int code_size = sizeof(T)*8;     
        const int start_idx = start_shift_pos/code_size;   
        if(add_block) block_vec.push_back(static_cast<T>(0));
        const int block_vec_size = block_vec.size();
        for(int i=start_idx;i<block_vec_size;++i){
            temp_code_vec[i-start_idx] = block_vec[i]>>shift_len;
            temp_code_vec[i-start_idx] |= (i==0 ? static_cast<T>(0) : block_vec[i-1])<<(code_size - shift_len);
        }
        const int start_shift_pos_inblock = start_shift_pos%code_size;
        if(start_shift_pos_inblock+shift_len <= code_size){           
            const int valid_bit_size = code_size - start_shift_pos_inblock - shift_len;
            const int white_size = code_size - start_shift_pos_inblock;
            T white_mask = (static_cast<T>(-1))>>start_shift_pos_inblock;                 
            T shift_mask = (static_cast<T>(1)<<valid_bit_size)-1;            
            T right_remain = ((block_vec[start_idx]>>shift_len)&shift_mask);
            temp_code_vec[0] = block_vec[start_idx];
            temp_code_vec[0] &= (~white_mask);
            temp_code_vec[0] |= right_remain;
        }else{
            assert(start_idx+1 < block_vec.size());
            const int left_white_size = code_size - start_shift_pos_inblock;
            const int right_white_size = start_shift_pos_inblock+shift_len-code_size;
            T left_white_mask = (static_cast<T>(1)<<left_white_size)-1;
            T right_white_mask = static_cast<T>(-1)<<(code_size - right_white_size);  
            temp_code_vec[0] = block_vec[start_idx];
            temp_code_vec[0] &= (~left_white_mask);
            temp_code_vec[1] &= (~right_white_mask);
        }
        std::copy(temp_code_vec.begin(),temp_code_vec.begin()+(block_vec_size - start_idx),block_vec.begin()+start_idx);
        total_block_bits += shift_len;
    }       
    void insert(size_t pos,const Ch& ch,const auto& encoder){        
        if constexpr(DEBUG) assert(pos <= block_size);        
        constexpr int code_size = sizeof(T)*8;     
        auto [code,len] = encoder.encode(ch);      
        
        if(!is_valid_cache()){            
            validate_cache(encoder);
        }

        const int start_shift_pos = cached_pos_vec[pos];        
        cached_pos_vec.insert(cached_pos_vec.begin()+(pos+1),cached_pos_vec[pos]);
        std::transform(cached_pos_vec.begin()+(pos+1),cached_pos_vec.end(),cached_pos_vec.begin()+(pos+1),
        [len](auto val){return val+len;});

        shift_right(start_shift_pos,len);        
        const int start_shift_pos_inblock = start_shift_pos%code_size;
        const int start_idx = start_shift_pos/code_size;        
        if(start_shift_pos_inblock+len <= code_size){            
            code>>=start_shift_pos_inblock;
            block_vec[start_idx] |= code;
        }else{
            auto left_code = code>>start_shift_pos_inblock;
            auto right_code = code<<(code_size - start_shift_pos_inblock);
            block_vec[start_idx] |= left_code;
            block_vec[start_idx+1] |= right_code;
        }
        ++block_size;        
        const int lastblock_valid_bits = (total_block_bits%code_size == 0 ? code_size : total_block_bits%code_size);
        T white = static_cast<T>(-1)<<(code_size - lastblock_valid_bits);
        block_vec.back() &= white;
    }

    auto erase(size_t pos,const auto& encoder){
        if constexpr(DEBUG) assert(pos < block_size);        
        constexpr int code_size = sizeof(T)*8;     
        if(!is_valid_cache()){
            validate_cache(encoder);
        }
        const int start_shift_pos_minus_one = cached_pos_vec[pos];
        //const int start_shift_pos_minus_one = get_start_shift_pos(pos,encoder);
        const int start_idx = start_shift_pos_minus_one/code_size;
        const int left_bits = start_shift_pos_minus_one%code_size;
        const int curr_block_size = block_vec.size();        
        const T code = ((block_vec[start_idx]<<left_bits) | ((left_bits==0 || start_idx==curr_block_size-1) ? static_cast<T>(0) : block_vec[start_idx+1]>>(code_size - left_bits)));
        auto [erased_ch,erasecode_len] = encoder.decode(code);
        //const int erasecode_len = encoder.decode(code).second;

        cached_pos_vec.erase(cached_pos_vec.begin()+(pos+1));
        std::transform(cached_pos_vec.begin()+(pos+1),cached_pos_vec.end(),cached_pos_vec.begin()+(pos+1),
        [erasecode_len](auto val){return val-erasecode_len;});

        //cout<<"erasecode len: "<<erasecode_len<<" ,left bits: "<<left_bits<<'\n';
        shift_left(start_shift_pos_minus_one+erasecode_len,erasecode_len);
        --block_size;
        const int lastblock_valid_bits = (total_block_bits%code_size == 0 ? code_size : total_block_bits%code_size);
        T white = static_cast<T>(-1)<<(code_size - lastblock_valid_bits);
        block_vec.back() &= white;
        return erased_ch;
    }
    void replace(size_t pos,const Ch& ch,const auto& encoder){
        if constexpr(DEBUG) assert(0 <= pos && pos < block_size);        
        const int code_size = sizeof(T)*8; 
        const int start_shift_pos_minus_one = get_start_shift_pos(pos,encoder);
        const int start_idx = start_shift_pos_minus_one/code_size;
        const int left_bits = start_shift_pos_minus_one%code_size;
        const int curr_block_size = block_vec.size();        
        const T code = ((block_vec[start_idx]<<left_bits) | ((left_bits==0 || start_idx==curr_block_size-1) ? static_cast<T>(0) : block_vec[start_idx+1]>>(code_size - left_bits)));
        const int prevcode_len = encoder.decode(code).second;
        auto [currcode,currcode_len] = encoder.encode(ch);        
        //std::cout<<total_block_bits<<','<<start_shift_pos_minus_one<<','<<static_cast<int>(prevcode_len)<<','<<static_cast<int>(currcode_len)<<std::endl;
        if(left_bits + prevcode_len <= code_size){
            T white = ((static_cast<T>(1)<<prevcode_len)-1)<<(code_size - left_bits - prevcode_len);
            block_vec[start_idx] &= (~white);            
        }else{
            T left_white = static_cast<T>(-1)>>left_bits;
            T right_white = static_cast<T>(-1)>>(left_bits + prevcode_len - code_size);
            block_vec[start_idx] &= (~left_white);
            block_vec[start_idx+1] &= right_white;
        }        
        if(currcode_len < prevcode_len) shift_left(start_shift_pos_minus_one+prevcode_len,prevcode_len - currcode_len);
        else if(prevcode_len < currcode_len) shift_right(start_shift_pos_minus_one+prevcode_len,currcode_len - prevcode_len);        
        if(left_bits + currcode_len <= code_size){
            currcode>>=left_bits;
            block_vec[start_idx] |= currcode;
        }else{
            auto left_code = currcode>>left_bits;
            auto right_code = currcode<<(code_size - left_bits);
            block_vec[start_idx] |= left_code;
            block_vec[start_idx+1] |= right_code;            
        }
        const int lastblock_valid_bits = (total_block_bits%code_size == 0 ? code_size : total_block_bits%code_size);
        T white = static_cast<T>(-1)<<(code_size - lastblock_valid_bits);
        block_vec.back() &= white;
    }
    std::vector<Ch> get(const auto& encoder) const {
        assert(block_size > 0);
        return decode_block<T,Ch>(block_vec,block_size,encoder);
    }
    Ch at(size_t pos,const auto& encoder) const {
        if constexpr(DEBUG){
            assert(pos>=0 && pos<block_size);
        }
        const int code_size = sizeof(T)*8; 
        const int start_shift_pos_minus_one = get_start_shift_pos(pos,encoder);
        const int start_idx = start_shift_pos_minus_one/code_size;
        const int left_bits = start_shift_pos_minus_one%code_size;
        const int curr_block_size = block_vec.size();        
        const T code = ((block_vec[start_idx]<<left_bits) | ((left_bits==0 || start_idx==curr_block_size-1) ? static_cast<T>(0) : block_vec[start_idx+1]>>(code_size - left_bits)));
        auto [ch,len] = encoder.decode(code);
        return ch;
    }
    size_t use_bytes() const{
        return block_vec.size()*sizeof(T);
    }
    size_t size() const {        
        return block_size;
    }
    void replace_block(const auto& block,const auto& encoder){        
        block_size = block.size();
        assert(block_size > 0);
        auto [vec,len] = encode_block<T,Ch>(block,encoder);
        block_vec = std::move(vec);
        total_block_bits = len;
    }

    void merge(auto rhs_unique_ptr,const auto& left_encoder,const auto& right_encoder){
        //assert(false);
        auto decoded_left = decode_block<T,Ch>(block_vec,block_size,left_encoder);
        auto decoded_right = rhs_unique_ptr->get(right_encoder);        
        //std::cout<<block_size<<" , "<<decoded_right.size()<<std::endl;
        block_size+=decoded_right.size();        
        decoded_left.insert(decoded_left.end(),decoded_right.begin(),decoded_right.end());
        auto [vec,len] = encode_block<T,Ch>(decoded_left,left_encoder);
        block_vec = std::move(vec);
        total_block_bits = len;
        if(is_valid_cache()) invalidate_cache();
        validate_cache(left_encoder);
    }
    auto split(size_t pos,const auto& encoder){        
        if constexpr(DEBUG) assert(pos>0 && pos<block_size);
        assert(is_valid_cache());
        //if(!is_valid_cache()) validate_cache(encoder);
        const int code_size = sizeof(T)*8; 
        size_t lsize = pos, rsize = block_size - pos;
        //const int split_bitpos = get_start_shift_pos(pos,encoder);
        const int split_bitpos = cached_pos_vec[lsize];
        HuffmanBlock<T,Ch,MAX_BLOCK_SIZE> r_block;
        const int split_bitpos_idx = split_bitpos/code_size;
        const int split_bitpos_remain = split_bitpos%code_size;
        std::vector<T> right_block_vec(block_vec.size() - split_bitpos_idx);        
        std::copy(block_vec.begin()+split_bitpos_idx,block_vec.end(),right_block_vec.begin());

        std::vector<int> right_cached_pos_vec(rsize+1);
        std::copy(cached_pos_vec.begin()+lsize,cached_pos_vec.end(),right_cached_pos_vec.begin());        
        std::transform(right_cached_pos_vec.begin(),right_cached_pos_vec.end(),right_cached_pos_vec.begin(),
        [split_bitpos](int val){return val - split_bitpos;});        

        r_block.block_size = rsize;
        r_block.total_block_bits = total_block_bits - split_bitpos + split_bitpos_remain;
        r_block.block_vec = std::move(right_block_vec);
        r_block.cached_pos_vec = std::move(right_cached_pos_vec);
        if(split_bitpos_remain>0)
            r_block.shift_left(0,split_bitpos_remain);        

        block_size -= rsize;
        //total_block_bits -= split_bitpos;
        total_block_bits = split_bitpos;
        block_vec.erase(block_vec.begin()+(split_bitpos_idx+1),block_vec.end());
        cached_pos_vec.erase(cached_pos_vec.begin()+(lsize+1),cached_pos_vec.end());
        T white = static_cast<T>(-1)<<(code_size - split_bitpos_remain);
        block_vec[split_bitpos_idx] &= white;

        return r_block;


        /*auto decoded = decode_block<T,Ch>(block_vec,block_size,encoder);
        size_t lsize = pos, rsize = block_size - pos;
        std::vector<Ch> l_vec(lsize);
        std::vector<Ch> r_vec(rsize);
        std::copy(decoded.begin(),decoded.begin()+lsize,l_vec.begin());
        std::copy(decoded.begin()+lsize,decoded.end(),r_vec.begin());
        HuffmanBlock<T,Ch,MAX_BLOCK_SIZE> l_block(l_vec,encoder);
        HuffmanBlock<T,Ch,MAX_BLOCK_SIZE> r_block(r_vec,encoder);
        return std::make_pair(std::move(l_block),std::move(r_block));*/
    }
};






template<typename T,int MAX_BLOCK_SIZE,typename large_ch = uint16_t,typename small_ch = uint8_t>
class SpecialBlock{
    size_t block_size;    
    size_t total_block_bits;
    std::vector<T> block_vec;
public:
    SpecialBlock() = default;
    explicit SpecialBlock(int size) : block_size(size),block_vec(size),total_block_bits(size){}    
    explicit SpecialBlock(const auto& block,const auto& encoder){
        block_size = block.size()*2;
        auto [vec,len] = encode_block<T,large_ch>(block,encoder);
        block_vec = std::move(vec);
        total_block_bits = len;
    }    
    

    auto block_convert(const std::vector<small_ch>& original_block) const {
        if(original_block.empty()) return std::vector<large_ch>{};
        std::vector<large_ch> converted_block((original_block.size()+1)/2);
        ::memcpy(converted_block.data(),original_block.data(),original_block.size());
        return converted_block;
    }
    auto block_deconvert(const std::vector<large_ch>& converted_block,int original_block_size) const{
        if(converted_block.empty()) return std::vector<small_ch>{};
        std::vector<small_ch> original_block;
        original_block.reserve(original_block_size+1);
        original_block.resize(original_block_size);
        ::memcpy(original_block.data(),converted_block.data(),original_block_size);
        return original_block;
    }
    explicit SpecialBlock(const std::vector<large_ch>& block,const auto& encoder){
        block_size = block.size()*2;        
        auto [vec,len] = encode_block<T,large_ch>(block,encoder);
        block_vec = std::move(vec);
        total_block_bits = len;
    }
    size_t use_bytes() const{
        return block_vec.size()*sizeof(T);
    }
    size_t size() const {        
        return block_size;
    }
    void setSize(int new_size){
        block_size = new_size;
    }
    std::vector<large_ch> get(const auto& encoder) const {
        return decode_block<T,large_ch>(block_vec,(block_size+1)/2,encoder);
    }
    void replace_block(const auto& block,const auto& encoder){        
        auto [vec,len] = encode_block<T,large_ch>(block,encoder);
        block_vec = std::move(vec);
        total_block_bits = len;
    }
    void insert(const auto pos,const small_ch ch,const auto& encoder){
        if constexpr(DEBUG) assert(pos <= block_size);
        auto decoded = decode_block<T,large_ch>(block_vec,(block_size+1)/2,encoder);
        auto original_block = block_deconvert(decoded,block_size);
        original_block.insert(original_block.begin()+pos,ch);
        auto converted_block = block_convert(original_block);
        auto [vec,len] = encode_block<T,large_ch>(converted_block,encoder);
        block_vec = std::move(vec);
        total_block_bits = len;
        ++block_size;
    }
    auto erase(const auto pos,const auto& encoder){
        if constexpr(DEBUG) assert(pos < block_size);
        auto decoded = decode_block<T,large_ch>(block_vec,(block_size+1)/2,encoder);
        auto original_block = block_deconvert(decoded,block_size);
        auto erase_ch = original_block[pos];
        original_block.erase(original_block.begin()+pos);
        auto converted_block = block_convert(original_block);
        auto [vec,len] = encode_block<T,large_ch>(converted_block,encoder);
        block_vec = std::move(vec);
        total_block_bits = len;
        --block_size;
        return erase_ch;
    }
    void merge(auto rhs_unique_ptr,const auto& left_encoder,const auto& right_encoder){
        auto decoded_left = decode_block<T,large_ch>(block_vec,(block_size+1)/2,left_encoder);
        auto original_block_left = block_deconvert(decoded_left,block_size);
        auto decoded_right = rhs_unique_ptr->get(right_encoder);        
        auto original_block_right = block_deconvert(decoded_right,rhs_unique_ptr->size());
        original_block_left.insert(original_block_left.end(),original_block_right.begin(),original_block_right.end());
        auto converted_block_merged = block_convert(original_block_left);
        auto [vec,len] = encode_block<T,large_ch>(converted_block_merged,left_encoder);
        block_vec = std::move(vec);
        block_size += rhs_unique_ptr->size();
        total_block_bits = len;
    }
    auto split(size_t pos,const auto& encoder){
        if constexpr(DEBUG) assert(pos>0 && pos<block_size);
        assert(pos%2==0);
        size_t lsize = pos, rsize = block_size - pos;
        auto decoded = decode_block<T,large_ch>(block_vec,(block_size+1)/2,encoder);
        std::vector<large_ch> l_vec(lsize/2);
        std::vector<large_ch> r_vec((rsize+1)/2);
        std::copy(decoded.begin(),decoded.begin()+(lsize/2),l_vec.begin());
        std::copy(decoded.begin()+(lsize/2),decoded.end(),r_vec.begin());
        SpecialBlock<T,MAX_BLOCK_SIZE> l_block(l_vec,encoder);
        SpecialBlock<T,MAX_BLOCK_SIZE> r_block(r_vec,encoder);
        
        if(rsize%2==1) r_block.setSize(rsize);

        return std::make_pair(std::move(l_block),std::move(r_block));
    }
};

#endif