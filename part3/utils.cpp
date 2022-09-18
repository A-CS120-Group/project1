#include "utils.h"

std::vector<double> linspace(double min, double max, int n) {
    std::vector<double> result;
    result.reserve(n);
    int it = 0;
    for (double interval = (max - min) / (n - 1); it < n - 1; it++) { result.push_back(min + it * interval); }
    result.push_back(max);
    return result;
}

std::vector<double> cumtrapz(std::vector<double> t, std::vector<double> f) {
    //	assert(t.size() == f.size());
    size_t size = t.size();
    std::vector<double> result;
    result.reserve(size);
    double total = 0.0;
    double last = *t.begin();
    result.push_back(0);
    for (int k = 0; k < size - 1; ++k) {
        double d;
        double s_tmp;
        d = t[k + 1] - t[k];
        t[k] = d;
        s_tmp = f[k + 1];
        total += d * ((last + s_tmp) / 2.0);
        last = s_tmp;
        result.push_back(total);
    }
    return result;
}

static void vecBool2string(std::vector<bool> src, char *des) {
    auto length = src.size();
    for (int i = 0; i < floor(length / 8); ++i) {
        char q = 0;
        for (int j = 0; j < 8; ++j) {
            q <<= 1;
            q = src[i * 8 + j] ? (char) (q + 1) : (char) q;
        }
        des[i] = q;
    }
}

int crc8(const std::vector<bool> &source) {
    boost::crc_basic<8> crc8{0XA7, 0X00, 0, false, false};
    auto sourceString = new char[source.size() / 8];
    vecBool2string(source, sourceString);
    crc8.process_bytes(sourceString, source.size() / 8);
    return crc8.checksum();
}

std::vector<double> smooth(const std::vector<double> &y, size_t span) {
    if (span == 0) { return y; }
    auto size = y.size();
    auto smoothSize = ((span - 1) | 1);
    auto result = std::vector<double>{};
    result.reserve(size);
    for (size_t pos = 0; pos < size; ++pos) {
        auto maxDistToEdge = std::min({smoothSize / 2, pos, size - pos - 1});
        result.push_back(y[pos]);
        for (auto i = 1; i <= maxDistToEdge; ++i) {
            result[pos] += y[pos - i];
            result[pos] += y[pos + i];
        }
        result[pos] /= (double) (maxDistToEdge * 2 + 1);
    }
    return result;
}