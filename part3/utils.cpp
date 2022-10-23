#include "utils.h"

decodeDouble::decodeDouble() : num() {}

decodeDouble::decodeDouble(double x) : num(x) {}

Fixed::Fixed() : l() {}

Fixed::Fixed(int x) : l((long long) x << 32) {}

Fixed::Fixed(long long x) : l(x) {}

Fixed::Fixed(double x) : l() {
    decodeDouble xd(x);
    if (xd.field.mantissa == 0 && xd.field.exponent == 0) {
        l = 0;
        return;
    }
    unsigned long long xv = (1uLL << 52) | xd.field.mantissa;
    int shift = (int) (xd.field.exponent) - 1043;
    if (shift <= -64 || shift >= 64) {
        l = 0;
        return;
    }
    l = (long long) (shift > 0 ? xv << shift : xv >> -shift);
    if (xd.field.sign)
        l = -l;
}

double Fixed::to_double() const {
    decodeDouble xd;
    long long lv = l;
    if (lv == 0)
        return 0.0;
    if (lv < 0)
        xd.field.sign = 1, lv = -lv;
    else
        xd.field.sign = 0;
    int shift = 11 - countLeadingZero(lv);
    if (shift <= -64 || shift >= 64)
        xd.field.mantissa = 0;
    else
        xd.field.mantissa = shift > 0 ? lv >> shift : lv << -shift;
    xd.field.exponent = shift + 1043;
    return xd.num;
}

Fixed Fixed::operator+(Fixed x) const { return Fixed(l + x.l); }

Fixed Fixed::operator-(Fixed x) const { return Fixed(l - x.l); }

Fixed Fixed::operator*(Fixed x) const {
    long long al = l < 0 ? -l : l;
    long long bl = x.l < 0 ? -x.l : x.l;
    long long ah = al >> 32;
    long long bh = bl >> 32;
    al &= 0xffffffffLL;
    bl &= 0xffffffffLL;
    long long r = ((al * bl) >> 32) + al * bh + ah * bl + ((ah * bh) << 32);
    return Fixed((l < 0) == (x.l < 0) ? r : -r);
}

//Fixed Fixed::operator/(int x) const { return operator*(Fixed(1.0 / x)); }

Fixed Fixed::operator-() const{
    return Fixed(-l);
}

bool Fixed::operator>(Fixed x) const { return operator-(x).l > 0; }

int countLeadingZero(unsigned long long x) {
    int r = 0;
    if (!(x & 0xFFFFFFFF00000000))
        r += 32, x <<= 32;
    if (!(x & 0xFFFF000000000000))
        r += 16, x <<= 16;
    if (!(x & 0xFF00000000000000))
        r += 8, x <<= 8;
    if (!(x & 0xF000000000000000))
        r += 4, x <<= 4;
    if (!(x & 0xC000000000000000))
        r += 2, x <<= 2;
    if (!(x & 0x8000000000000000))
        r += 1;
    return r;
}

static void vecBool2string(std::vector<bool> src, char *des) {
    auto length = src.size();
    for (int i = 0; i < length / 8; ++i) {
        char q = 0;
        for (int j = 0; j < 8; ++j) {
            q <<= 1;
            q = src[i * 8 + j] ? (char) (q + 1) : (char) q;
        }
        des[i] = q;
    }
}

int crc8(const std::vector<bool> &source) {
    boost::crc_basic<8> crc8{
            0XA7, 0X00, 0, false, false
    };
    auto sourceString = new char[source.size() / 8];
    vecBool2string(source, sourceString);
    crc8.process_bytes(sourceString, source.size() / 8);
    return crc8.checksum();
}

std::vector<Fixed> smooth(const std::vector<Fixed> &y, size_t span) {
    if (span == 0) {
        return y;
    }
    auto size = y.size();
    auto smoothSize = ((span - 1) | 1);
    std::vector<Fixed> result;
    result.reserve(size);
    for (size_t pos = 0; pos < size; ++pos) {
        int maxDistToEdge = (int) std::min({smoothSize / 2, pos, size - pos - 1});
        result.push_back(y[pos]);
        for (int i = 1; i <= maxDistToEdge; ++i)
            result[pos] = result[pos] + y[pos - i] + y[pos + i];
        result[pos] = result[pos] * Fixed(1.0 / (maxDistToEdge * 2 + 1));
    }
    return result;
}

std::string getPath(std::string path, int depth) {
    for (int i = 0; i < depth; ++i) {
        FILE *file = fopen(path.c_str(), "r");
        if (file) {
            fclose(file);
            return path;
        }
        path = "../" + path;
    }
    exit(1);
}

//std::vector<double> linspace(double min, double max, int n) {
//    std::vector<double> result;
//    result.reserve(n);
//    int it = 0;
//    for (double interval = (max - min) / (n - 1); it < n - 1; it++) { result.push_back(min + it * interval); }
//    result.push_back(max);
//    return result;
//}
//
//std::vector<double> cumtrapz(std::vector<double> t, std::vector<double> f) {
//    //	assert(t.size() == f.size());
//    size_t size = t.size();
//    std::vector<double> result;
//    result.reserve(size);
//    double total = 0.0;
//    double last = *t.begin();
//    result.push_back(0);
//    for (int k = 0; k < size - 1; ++k) {
//        double d;
//        double s_tmp;
//        d = t[k + 1] - t[k];
//        t[k] = d;
//        s_tmp = f[k + 1];
//        total += d * ((last + s_tmp) / 2.0);
//        last = s_tmp;
//        result.push_back(total);
//    }
//    return result;
//}
