//
// Created by Yuriy Baranov on 2019-02-25.
//
#include <Poco/Exception.h>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include "hamming_code.h"


void test1() {
    constexpr int wordSize = 4;
    HammingCode<wordSize> h;

    auto msg = std::string("1011");
    std::reverse(msg.begin(), msg.end());
    std::cout << "message: " << msg << std::endl;
    auto message = std::bitset<wordSize>(msg);


    auto encoded = h.encode(message);
    auto encodedS = encoded.to_string();
    std::string correct = "00110011";
    std::reverse(correct.begin(), correct.end());
    if (encodedS != correct) {
        std::cout << "encoded != correct: " << encodedS << ' ' << correct << std::endl;
        throw Poco::Exception("failed test 1");
    }

    std::cout << "test error count 0" << std::endl;
    auto decodingResult = h.decode(encoded);
    auto decoded = decodingResult.first;
    if (decodingResult.second != 0) {
        std::cout << "wrong detected error count != 0: " << decodingResult.second << std::endl;
        throw Poco::Exception("failed test 1");
    }
    if (decoded != message) {
        std::cout << "decoded != message: " << decoded << ' ' << message << std::endl;
        throw Poco::Exception("failed test 1");
    }

    std::cout << "test error count 1" << std::endl;
    for (size_t i = 0; i < h.getBlockSize(); i++) {
        auto encodedWithOneError = encoded;
        encodedWithOneError.flip(i);
        decodingResult = h.decode(encodedWithOneError);
        if (decodingResult.second != 1) {
            std::cout << "wrong detected error count != 1: " << decodingResult.second << std::endl;
            throw Poco::Exception("failed test 1");
        }
        if (decodingResult.first != message) {
            std::cout << "decoded != message: " << decoded << ' ' << message << std::endl;
            throw Poco::Exception("failed test 1");
        }
    }

    std::cout << "test error count 2" << std::endl;
    for (size_t i = 0; i < h.getBlockSize(); i++) {
        for (size_t j = 0; j < h.getBlockSize(); j++) {
            if (i == j) continue;
            auto encodedWithTwoErrors = encoded;
            encodedWithTwoErrors.flip(i);
            encodedWithTwoErrors.flip(j);
            decodingResult = h.decode(encodedWithTwoErrors);
            if (decodingResult.second != 2) {
                std::cout << "wrong detected error count != 2: " << decodingResult.second << std::endl;
                throw Poco::Exception("failed test 1");
            }
        }
    }

    std::cout << "passed test 1" << std::endl;
}

template <int wordSize>
std::bitset<wordSize> randomBitSet() {
    std::bitset<wordSize> b;
    for (int i = 0; i < wordSize; i++) {
        b[i] = rand() % 2;
    }
    return b;
}

template <int wordSize>
void randomTest(HammingCode<wordSize>& h) {
    auto message = randomBitSet<wordSize>();

    auto errorText = std::string("failed random test for ") + message.to_string();
    auto encoded = h.encode(message);
    auto decodingResult = h.decode(encoded);
    auto decoded = decodingResult.first;

    if (decodingResult.second != 0) {
        std::cout << "wrong detected error count != 0: " << decodingResult.second << std::endl;
        throw Poco::Exception(errorText);
    }
    if (decoded != message) {
        std::cout << "for error count 0 decoded != message" << std::endl;
        throw Poco::Exception(errorText);
    }

    for (size_t i = 0; i < h.getBlockSize(); i++) {
        auto encodedWithOneError = encoded;
        encodedWithOneError.flip(i);
        decodingResult = h.decode(encodedWithOneError);
        if (decodingResult.second != 1) {
            std::cout << "wrong detected error count != 1: " << decodingResult.second << std::endl;
            throw Poco::Exception(errorText);
        }
        if (decodingResult.first != message) {
            std::cout << "for error count 1  decoded != message" << std::endl;
            throw Poco::Exception(errorText);
        }
    }

    for (size_t i = 0; i < 100; i++) {
        size_t pos1 = rand() % h.getBlockSize(), pos2 = rand() % h.getBlockSize();
        if (pos1 == pos2) {
            continue;
        }
        auto encodedWithTwoErrors = encoded;
        encodedWithTwoErrors.flip(pos1);
        encodedWithTwoErrors.flip(pos2);
        decodingResult = h.decode(encodedWithTwoErrors);
        if (decodingResult.second != 2) {
            std::cout << "wrong detected error count != 2: " << decodingResult.second << std::endl;
            throw Poco::Exception(errorText);
        }
    }
}

template <int wordSize>
void randomTest() {
    HammingCode<wordSize> h;
    const int N = 10;
    for (int i = 0; i < N; i++) {
        randomTest(h);
    }
}

void stressTest() {
    // will not make constexpr loop for now
    randomTest<0>();
    randomTest<1>();
    randomTest<2>();
    randomTest<3>();
    randomTest<4>();
    randomTest<5>();
    randomTest<5>();
    randomTest<6>();
    randomTest<7>();
    randomTest<8>();
    randomTest<9>();
    randomTest<10>();
    randomTest<11>();
    randomTest<12>();
    randomTest<13>();
    randomTest<14>();
    randomTest<15>();
    randomTest<16>();
    randomTest<17>();
    randomTest<18>();
    randomTest<19>();
    randomTest<20>();
    randomTest<21>();
    randomTest<22>();
    randomTest<23>();
    randomTest<24>();
    randomTest<25>();
    randomTest<26>();
    randomTest<27>();
    randomTest<28>();
    randomTest<29>();
    randomTest<30>();
    randomTest<31>();
    randomTest<100>();
    randomTest<500>();
    std::cout << "passed stress test" << std::endl;
}

template <int wordSize>
void getManyErrorsDetectionRatio() {
    HammingCode<wordSize> h;
    auto message = randomBitSet<wordSize>();
    auto errorText = std::string("failed random test for ") + message.to_string();
    auto encoded = h.encode(message);

    const int N = 100000;
    std::cout << "\ngetting many errors detection ratio for word size " << wordSize << std::endl;
    std::cout << "errors | detected | not detected | single (mistaken) | double (mistaken)" << std::endl;
    for (int errorCount : {3, 4, 5}) {
        if (errorCount > wordSize) {
            return;
        }
        std::unordered_map<int, int> detected;
        for (int i = 0; i < N; i++) {
            std::vector<int> allPositions((size_t) h.getBlockSize());
            for (int j = 0; j < allPositions.size(); j++) {
                allPositions[j] = j;
            }
            auto encodedWithThreeErrors = encoded;
            for (int j = 0; j < errorCount; j++) {
                int pos = rand() % allPositions.size();
                int errorPos = allPositions[pos];
                allPositions.erase(allPositions.begin() + pos);
                encodedWithThreeErrors.flip(errorPos);
            }

            auto decodingResult = h.decode(encodedWithThreeErrors);
            detected[decodingResult.second] += 1;
        }
        std::cout << std::setprecision(3) << std::fixed << "   " << errorCount << "   |   " << (float) detected[-1] / N << "  |   " <<
            (float) detected[0] / N << "      |       " << (float) detected[1] / N <<  "       |   " << (float) detected[2] / N << std::endl;
    }
}

int main() {
    test1();
    stressTest();
    getManyErrorsDetectionRatio<4>();
    getManyErrorsDetectionRatio<5>();
    getManyErrorsDetectionRatio<25>();
    getManyErrorsDetectionRatio<33>();
    getManyErrorsDetectionRatio<100>();
}
