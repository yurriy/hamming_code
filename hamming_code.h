//
// Created by Yuriy Baranov on 2019-02-25.
//

#ifndef PROJECT_HAMMING_CODE_H
#define PROJECT_HAMMING_CODE_H

#endif //PROJECT_HAMMING_CODE_H

#include <bitset>
#include <iostream>

template <int wordSize>
class HammingCode {
    /// Hamming code with extra parity bit for double errors detection
private:
    static constexpr int getParityBitsCount(int minK) noexcept {
        if (1ull << minK >= wordSize + minK + 1) {
            return minK;
        } else {
            return getParityBitsCount(minK + 1);
        }
    }

    static constexpr int parityBitsCount=getParityBitsCount(0) + 1;
    static constexpr int blockSize=wordSize + parityBitsCount;

public:
    std::bitset<blockSize> encode(std::bitset<wordSize>& word) const {
        std::bitset<blockSize> block;

        for (int i = 0, j = 0; i < blockSize; i++) {
            // check if it is a parity bit position (0 or power of 2)
            if ((i & (i - 1)) == 0) {
                continue;
            }
            block[i] = word[j++];
        }

        for (int i = 0; i < parityBitsCount - 1; i++) {
            int sum = 0;
            int parityBitMask = (1 << i);
            for (int j = 0; j < blockSize; j++) {
                if (j & parityBitMask) {
                    sum += block[j];
                }
            }
            block[1 << i] = sum % 2;
        }

        // overall parity bit
        block[0] = block.count() % 2;

        return block;
    }

    std::pair<std::bitset<wordSize>, int> decode(std::bitset<blockSize> block) const {
        std::bitset<parityBitsCount - 1> parity;
        for (int i = 0; i < parityBitsCount - 1; i++) {
            int sum = 0;
            int parityBitMask = (1 << i);
            for (int j = 0; j < blockSize; j++) {
                if (j & parityBitMask) {
                    sum += block[j];
                }
            }
            parity[i] = sum % 2;
        }

        int errorsCount = 0;
        bool overallParity = (bool) (block.count() % 2);
        if (overallParity) {
            auto errorIndex = parity.to_ulong();
            if (errorIndex >= blockSize) {
                errorsCount = -1;
            } else {
                block.flip(errorIndex);
                errorsCount = 1;
            }
        } else if (parity.count()) {
            errorsCount = 2;
        }

        std::bitset<wordSize> word;
        for (int i = 0, j = 0; i < blockSize; i++) {
            // check if it is a parity bit position (0 or power of 2)
            if ((i & (i - 1)) == 0) {
                continue;
            }
            word[j++] = block[i];
        }
        return std::make_pair(word, errorsCount);
    }

    constexpr int getWordSize() const {
        return wordSize;
    }

    constexpr int getParityBitsCount() const {
        return parityBitsCount;
    }

    constexpr int getBlockSize() const {
        return blockSize;
    }
};

using HammingCode33 = HammingCode<33>;
