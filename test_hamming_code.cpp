//
// Created by Yuriy Baranov on 2019-02-25.
//
#include <Poco/Exception.h>
#include <Poco/Logger.h>
#include <Poco/ConsoleChannel.h>
#include <Poco/Bugcheck.h>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include "hamming_code.h"


Poco::Logger& logger = Poco::Logger::get("test_hamming_code");

void test1() {
    constexpr int wordSize = 4;
    HammingCode<wordSize> h;

    auto msg = std::string("1011");
    std::reverse(msg.begin(), msg.end());
    logger.information("message: %s", msg);
    auto message = std::bitset<wordSize>(msg);


    auto encoded = h.encode(message);
    auto encodedS = encoded.to_string();
    std::string correct = "00110011";
    std::reverse(correct.begin(), correct.end());

    poco_assert_msg(encodedS == correct, Poco::format("encoded: %s, correct: %s", encodedS, correct).data());

    logger.information("test error count 0");
    auto decodingResult = h.decode(encoded);
    auto decoded = decodingResult.first;

    poco_assert_msg(decodingResult.second == 0, Poco::format("detected error count: %d", decodingResult.second).data());
    poco_assert_msg(decoded == message, Poco::format("decoded: %s, message: %s", decoded, message).data());

    logger.information("test error count 1");
    for (size_t i = 0; i < h.getBlockSize(); i++) {
        auto encodedWithOneError = encoded;
        encodedWithOneError.flip(i);
        decodingResult = h.decode(encodedWithOneError);
        poco_assert_msg(decodingResult.second == 1, Poco::format("detected error count %d", decodingResult.second).data());
        poco_assert_msg(decodingResult.first == message, Poco::format("decoded: %s, message: %s", decodingResult.first, message).data());
    }

    logger.information("test error count 2");
    for (size_t i = 0; i < h.getBlockSize(); i++) {
        for (size_t j = 0; j < h.getBlockSize(); j++) {
            if (i == j) continue;
            auto encodedWithTwoErrors = encoded;
            encodedWithTwoErrors.flip(i);
            encodedWithTwoErrors.flip(j);
            decodingResult = h.decode(encodedWithTwoErrors);
            poco_assert_msg(decodingResult.second == 2, Poco::format("detected error count %d", decodingResult.second).data());
        }
    }
    logger.information("passed test 1");
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

    auto errorText = Poco::format("failed random test for %s", message.to_string());
    auto encoded = h.encode(message);
    auto decodingResult = h.decode(encoded);
    auto decoded = decodingResult.first;

    poco_assert_msg(decodingResult.second == 0, Poco::format("detected error count: %d", decodingResult.second).data());
    poco_assert_msg(decoded == message, Poco::format("decoded: %s, message: %s", decoded, message).data());

    for (size_t i = 0; i < h.getBlockSize(); i++) {
        auto encodedWithOneError = encoded;
        encodedWithOneError.flip(i);
        decodingResult = h.decode(encodedWithOneError);
        poco_assert_msg(decodingResult.second == 1, Poco::format("detected error count %d", decodingResult.second).data());
        poco_assert_msg(decodingResult.first == message, Poco::format("decoded: %s, message: %s", decodingResult.first, message).data());
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
        poco_assert_msg(decodingResult.second == 2, Poco::format("detected error count %d", decodingResult.second).data());
    }
}

template <int wordSize>
void randomTest() {
    HammingCode<wordSize> h;
    const int N = 100;
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
    logger.information("passed stress test");
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
    logger.setChannel(new Poco::ConsoleChannel(std::cout));
    test1();
    stressTest();
    getManyErrorsDetectionRatio<4>();
    getManyErrorsDetectionRatio<5>();
    getManyErrorsDetectionRatio<25>();
    getManyErrorsDetectionRatio<33>();
    getManyErrorsDetectionRatio<100>();
}
