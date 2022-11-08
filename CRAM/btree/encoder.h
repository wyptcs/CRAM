#ifndef ENCODER_H_
#define ENCODER_H_

#include <optional>
#include <map>
#include <vector>
#include <queue>
#include <algorithm>
#include <string>
#include <iostream>
#include <cmath>

class NaiveEncoder{};

const int INF_LEN = 100;

template<typename code_t,int MODE>
class CodeAllocator{
public:
    CodeAllocator() = default;
    CodeAllocator(int max_depth_) : max_depth(max_depth_),required_len(1<<(max_depth_+1)){        
        std::iota(required_len.begin(),required_len.end(),0);        
        std::transform(required_len.begin()+2,required_len.end(),required_len.begin()+2,
        [](auto val){return std::log2(val);});        
    }    
    auto allocateCode(int code_size){        
        if(code_size < required_len[1]) return std::optional<code_t>();
        auto [code,pos] = findCodePos(code_size);
        recalcLength(pos);
        return std::optional<code_t>(code);
    }
    auto get_bytes() const {
        return required_len.size()*sizeof(int);
    }
private:
    inline int parent(int pos) const { return pos/2;}
    inline int left_child(int pos) const {return pos*2;}
    inline int right_child(int pos) const {return pos*2+1;}
    void recalcLength(int pos){
        int l,r;
        required_len[pos] = INF_LEN;
        pos = parent(pos);
        while(pos){
            l = left_child(pos);
            r = right_child(pos);
            required_len[pos] = std::min(required_len[l],required_len[r]);
            pos = parent(pos);
        }
    }
    auto findCodePos(int size) const{
        int pos = 1;
        int l,r;
        code_t code = 3;
        code<<=(sizeof(code_t)*8-MODE);
        for(int i=1;i<=size;++i){
            l = left_child(pos);
            r = right_child(pos);
            if(size < required_len[l]){
                pos = r;
                code |= ((static_cast<code_t>(1))<<(sizeof(code_t)*8-i-MODE));                
            }else if(size < required_len[r]){
                pos = l;
            }else{
                pos = (required_len[r] <= required_len[l] ? l : r);
                if(pos==r) code |= ((static_cast<code_t>(1))<<(sizeof(code_t)*8-i-MODE));
            }
        }
        return std::make_pair(code,pos);
    }    

    int max_depth;
    std::vector<int> required_len;
};
template<typename code_t,typename ch_t>
class AdditionalCodeTable{
public:
    AdditionalCodeTable() = default;
    AdditionalCodeTable(const int SIGMA) : clen_v(SIGMA,INF_LEN){}
    void insert(code_t code,ch_t ch,uint8_t len){
        code_t prefix_newadded = 3;
        //prefix_newadded<<=(sizeof(code_t)*8-2);
        //assert((code&prefix_newadded) == prefix_newadded);
        tbl_extended_t item{code,ch,len};
        clen_v[ch] = len;
        tbl.insert(std::upper_bound(tbl.begin(),tbl.end(),item),item);  
        //std::cout<<"inserted code: "<<getBaseTwo(code,len)<<" ch: "<<ch<<" len: "<<static_cast<int>(len)<<'\n';
    }
    const uint8_t getCodeLen(ch_t ch) const{
        return clen_v[ch];
    }
    auto decode(code_t code) const{                
        tbl_extended_t code_for_search{code,0,0};
        auto code_it = std::prev(std::upper_bound(tbl.begin(),tbl.end(),code_for_search));           
        int shift_len = sizeof(code_t)*8 - code_it->len;            
        return std::make_pair(code_it->ch,code_it->len);
    }
    auto get_bytes() const {
        return tbl.size()*sizeof(tbl_extended_t) + clen_v.size();
    }
private:
    std::string getBaseTwo(code_t code,int len) const {
        std::string ret;
        code>>=(sizeof(code_t)*8 - len);
        for(int i=0;i<len;++i){
            ret+=(((code>>i)&1)==1 ? '1' : '0');
        }
        std::reverse(ret.begin(),ret.end());
        return ret;
    }
    struct tbl_extended_t{
        code_t code;
        ch_t ch;        
        uint8_t len;
        bool operator<(const tbl_extended_t& rhs)const{
            return code < rhs.code;
        }
    };
    std::vector<tbl_extended_t> tbl;
    std::vector<uint8_t> clen_v;
};


template<typename code_t = uint64_t,typename ch_t = uint16_t,int MODE = 0,int SIGMA = 65536>
class HuffmanEncoder{
public:       
    using clen_t = uint8_t;    
    using tbl_t = code_t;    
    using freq_t = uint64_t;
    auto get_bytes() const {
        auto ret = clen_v.size()*sizeof(clen_t)+code_v.size()*sizeof(code_t)+tbl.size()*sizeof(tbl_t)+tbl_extended.size()*sizeof(tbl_extended_t);
        if(0 < MODE){
            ret+=ca.get_bytes();
            ret+=act.get_bytes();
        }
        return ret;
    }
    
//private:
    int NILNODE = -1;    
    int TBL_WIDTH = 16;
    struct pq_block{
        freq_t freq;
        int ch;
        bool operator<(const pq_block& rhs) const{
            return freq > rhs.freq;
        }
    };
    struct tbl_extended_t{
        code_t code;
        ch_t ch;        
        bool operator<(const tbl_extended_t& rhs)const{
            return code < rhs.code;
        }
    };    
    std::vector<clen_t> clen_v;
    std::vector<code_t> code_v;
    std::vector<tbl_t> tbl;
    std::vector<tbl_extended_t> tbl_extended;
    CodeAllocator<code_t,MODE> ca;
    AdditionalCodeTable<code_t,ch_t> act;

    void make_code(std::vector<int>& left_child_v,std::vector<int>& right_child_v,int curr_node,int code_len,code_t code){
        if(left_child_v[curr_node]!=NILNODE){
            make_code(left_child_v,right_child_v,
            left_child_v[curr_node],code_len+1,code);
        }
        if(right_child_v[curr_node]!=NILNODE){
            make_code(left_child_v,right_child_v,
            right_child_v[curr_node],code_len+1,code+(static_cast<code_t>(1)<<(sizeof(code_t)*8-1-code_len)));
        }
        if(left_child_v[curr_node]==NILNODE && right_child_v[curr_node]==NILNODE){               
            clen_v[curr_node] = code_len;
            code_v[curr_node] = code;
        }
    }
    inline code_t get_tbl_code(code_t code) const {
        return code >> (sizeof(code_t)*8 - TBL_WIDTH);
    }
    void make_tbl(){
        std::map<code_t,ch_t> dic;
        
        for(int ch=0;ch<SIGMA;++ch){            
            const auto code = code_v[ch];            
            auto len = clen_v[ch];
            auto tbl_start = get_tbl_code(code);            
            if(len <= TBL_WIDTH){                
                auto block_size = (1<<(TBL_WIDTH - len));
                for(int i=tbl_start;i<tbl_start+block_size;++i){                    
                    tbl[i] = ch;
                }
            }else{
                tbl[tbl_start] = SIGMA;                
                dic[code] = ch;
            }
        }        
        if(dic.empty()) return;
        tbl_t index_start = 1,index_end = 1;
        auto curr_tbl_start = get_tbl_code(dic.begin()->first);
        for(auto& kvp:dic){                        
            tbl_extended.push_back({kvp.first,kvp.second});
            ++index_end;            
            if(curr_tbl_start!=get_tbl_code(kvp.first)){
                tbl[curr_tbl_start] = (index_start<<TBL_WIDTH)+index_end;
                curr_tbl_start = get_tbl_code(kvp.first);
                index_start = index_end;
            }
        }
        tbl[curr_tbl_start] = (index_start<<TBL_WIDTH)+index_end;
    }

    auto build_tree(auto start_leaf,auto end_leaf,auto start_nonleaf,const auto& freq,auto& left_child_v,auto& right_child_v){
        std::priority_queue<pq_block> pq;
        for(int i=start_leaf;i<end_leaf;++i){
            pq_block blk;blk.freq = freq[i];blk.ch = i;
            pq.push(blk);
        }
        auto curr_node = start_nonleaf;
        while(pq.size()>1){
            auto first_block = pq.top();pq.pop();
            auto second_block = pq.top();pq.pop();
            left_child_v[curr_node] = first_block.ch;
            right_child_v[curr_node] = second_block.ch;
            pq.push({first_block.freq+second_block.freq,curr_node});
            ++curr_node;
        }
        return curr_node;
    }
    auto getDividePos(const auto& freq){
        freq_t sum = std::reduce(freq.begin(),freq.end(),static_cast<freq_t>(0));
        auto freq_copy = freq;
        std::partial_sum(freq_copy.begin(),freq_copy.end(),freq_copy.begin());
        auto threshold_it = std::find_if(freq_copy.begin(),freq_copy.end(),[sum](const auto val){return val > sum*2/3;});
        return std::distance(freq_copy.begin(),threshold_it);
    }
    

public:
    HuffmanEncoder(){}
    explicit HuffmanEncoder(const auto& freq) : clen_v(SIGMA),code_v(SIGMA),tbl(SIGMA),
    TBL_WIDTH(std::log2(SIGMA)),ca(MODE>0 ? std::log2(SIGMA) : 0),act(SIGMA){                
        std::vector<int> left_child_v(SIGMA*2+3,NILNODE);
        std::vector<int> right_child_v(SIGMA*2+3,NILNODE);
        int curr_node = 0;        
        if constexpr(MODE==0){            
            auto root_node = build_tree(0,SIGMA,SIGMA,freq,left_child_v,right_child_v) - 1;            
            make_code(left_child_v,right_child_v,root_node,0,static_cast<code_t>(0));
        }else if constexpr(MODE==1){
            auto root_node = build_tree(0,SIGMA,SIGMA,freq,left_child_v,right_child_v) - 1;
            left_child_v[root_node+1] = root_node;
            make_code(left_child_v,right_child_v,root_node+1,0,static_cast<code_t>(0));                
        }else{
            auto divide_pos = getDividePos(freq);            
            auto left_top_node = build_tree(0,divide_pos,SIGMA,freq,left_child_v,right_child_v) - 1;
            auto right_top_node = build_tree(divide_pos,SIGMA,left_top_node+1,freq,left_child_v,right_child_v) - 1;            
            auto root_node = right_top_node + 2;
            auto root_right_child = right_top_node + 1;
            left_child_v[root_right_child] = right_top_node;
            left_child_v[root_node] = left_top_node;
            right_child_v[root_node] = root_right_child;
            make_code(left_child_v,right_child_v,root_node,0,static_cast<code_t>(0));            
        }        
        make_tbl();                
    }
    HuffmanEncoder(const HuffmanEncoder&) = default;
    HuffmanEncoder& operator=(const HuffmanEncoder&) = default;
    HuffmanEncoder(HuffmanEncoder&&) noexcept = default;
    HuffmanEncoder& operator=(HuffmanEncoder&&) noexcept = default;
    bool insertCode(const ch_t ch,int code_size){
        static_assert(MODE>0,"NOT SUPPORTED MODE ON INSERT CODE");
        auto allocated_code = ca.allocateCode(code_size-2);
        if(!allocated_code) return false;        
        //clen_v[ch] = code_size;
        code_v[ch] = allocated_code.value();
        act.insert(code_v[ch],ch,code_size);
        return true;
    }
    auto encode(const ch_t ch) const {        
        if constexpr(0 < MODE){
            return std::make_pair(code_v[ch],std::min(act.getCodeLen(ch),clen_v[ch]));
        }else{
            return std::make_pair(code_v[ch],clen_v[ch]);
        }        
    }
    auto decode(const code_t code) const {        
        if constexpr(0 < MODE){
            code_t prefix_newadded = 3;
            prefix_newadded<<=(sizeof(code_t)*8-MODE);
            if((code&prefix_newadded) == prefix_newadded){                
                return act.decode(code);
            }
        }
        auto tbl_idx = get_tbl_code(code);
        if(tbl[tbl_idx]<SIGMA){
            const auto ch = tbl[tbl_idx];
            return std::make_pair(static_cast<ch_t>(ch),clen_v[ch]);
        }               
        tbl_extended_t code_for_tbl_extended{code,0};
        auto tbl_code = std::prev(std::upper_bound(tbl_extended.begin(),tbl_extended.end(),code_for_tbl_extended));
        auto ch = tbl_code->ch;
        return std::make_pair(ch,clen_v[ch]);
    }
};

#endif