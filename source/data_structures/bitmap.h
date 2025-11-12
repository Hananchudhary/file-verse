#ifndef BITMAP_H
#define BITMAP_H
#include<iostream>
#include<cstdint>
#include<vector>
class Bitmap {
private:
    std::vector<uint8_t> bits;
    uint32_t total_blocks;
    
public:
    Bitmap(uint32_t num_blocks) : total_blocks(num_blocks) {
        bits.resize((num_blocks + 7) / 8, 0);
    }
    
    bool isBlockFree(uint32_t block_num) {
        if (block_num >= total_blocks) return false;
        uint32_t byte_idx = block_num / 8;
        uint32_t bit_idx = block_num % 8;
        return (bits[byte_idx] & (1 << bit_idx)) == 0;
    }
    
    void setBlockUsed(uint32_t block_num) {
        if (block_num >= total_blocks) return;
        uint32_t byte_idx = block_num / 8;
        uint32_t bit_idx = block_num % 8;
        bits[byte_idx] |= (1 << bit_idx);
    }
    
    void setBlockFree(uint32_t block_num) {
        if (block_num >= total_blocks) return;
        uint32_t byte_idx = block_num / 8;
        uint32_t bit_idx = block_num % 8;
        bits[byte_idx] &= ~(1 << bit_idx);
    }
    
    // Find N consecutive free blocks (returns first block number, or -1 if not found)
    int32_t findConsecutiveFreeBlocks(uint32_t n) {
        uint32_t consecutive = 0;
        uint32_t start_block = 0;
        
        for (uint32_t i = 0; i < total_blocks; i++) {
            if (isBlockFree(i)) {
                if (consecutive == 0) start_block = i;
                consecutive++;
                if (consecutive == n) return start_block;
            } else {
                consecutive = 0;
            }
        }
        return -1;
    }
    
    // Find N free blocks (not necessarily consecutive)
    std::vector<uint32_t> findFreeBlocks(uint32_t n) {
        std::vector<uint32_t> blocks;
        for (uint32_t i = 0; i < total_blocks && blocks.size() < n; i++) {
            if (isBlockFree(i)) {
                blocks.push_back(i);
            }
        }
        return blocks;
    }
    
    uint32_t countFreeBlocks() {
        uint32_t count = 0;
        for (uint32_t i = 0; i < total_blocks; i++) {
            if (isBlockFree(i)) count++;
        }
        return count;
    }
};

#endif