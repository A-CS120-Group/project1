#pragma once

#include <boost/crc.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <functional>
#include <cmath>

#define PI acos(-1)

std::vector<double> linspace(double min, double max, int n);

std::vector<double> cumtrapz(std::vector<double> t, std::vector<double> f);

int crc8(const std::vector<bool> &source);

std::vector<double> smooth(const std::vector<double> &y, size_t span);