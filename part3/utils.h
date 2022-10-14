#pragma once

#include <boost/crc.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <functional>

union decodeDouble {
    decodeDouble();
    explicit decodeDouble(double x);
    double num;
    struct {
        unsigned long long mantissa : 52;
        unsigned long long exponent : 11;
        unsigned long long sign : 1;
    } field;
};

class Fixed {
public:
    Fixed();
    explicit Fixed(int x);
    explicit Fixed(long long x);
    explicit Fixed(double x);
    [[nodiscard]] double to_double() const;
    Fixed operator+(Fixed x) const;
    Fixed operator-(Fixed x) const;
    Fixed operator*(Fixed x) const;
    //Fixed operator/(int x) const;
    Fixed operator-() const;
    bool operator>(Fixed x) const;
    long long l;
};

int countLeadingZero(unsigned long long x);

int crc8(const std::vector<bool> &source);

std::vector<Fixed> smooth(const std::vector<Fixed> &y, size_t span);

std::string getPath(std::string path, int depth);

//std::vector<double> linspace(double min, double max, int n);

//std::vector<double> cumtrapz(std::vector<double> t, std::vector<double> f);