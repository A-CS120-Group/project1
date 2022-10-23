#pragma once

#include <boost/crc.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <functional>

#define CRCL 32

union decodeDouble {
    decodeDouble();
    explicit decodeDouble(double x);
    double num;
    struct {
        unsigned long long mantissa: 52;
        unsigned long long exponent: 11;
        unsigned long long sign: 1;
    } field;
};

class Fixed {
public:
    long long l;
    Fixed();
    explicit Fixed(int x);
    explicit Fixed(long long x);
    explicit Fixed(double x);
    [[nodiscard]] double to_double() const;
    Fixed operator+(Fixed x) const;
    Fixed operator-(Fixed x) const;
    Fixed operator*(Fixed x) const;
    Fixed operator/(int x) const;
    Fixed operator-() const;
    bool operator>(Fixed x) const;
};

struct ECCResult{
    std::vector<bool> code;
    int hammingDistance;
};

class ECC{
public:
    std::vector<bool>code;
    std::vector<int> posError;
    std::vector<int> posMissing;
    int numError, numMissing, hammingDistance;
    std::vector<ECCResult> res;
    void searchError(int i);
    void searchMissing(int i);
    void search(const std::vector<bool>&co, int nE, const std::vector<int>&pE, const std::vector<int>&pM);
    void searchUninformed(const std::vector<bool>&co, int nE);
};

int countLeadingZero(unsigned long long x);

template<unsigned long long N>
unsigned int crc(const std::vector<bool> &source);

template<unsigned long long N>
bool crcCheck(std::vector<bool> source);

std::vector<Fixed> smooth(const std::vector<Fixed> &y, size_t span);
std::string getPath(std::string path, int depth);