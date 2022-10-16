#include "utils.h"

#define POINT 48 // Fixed decimal point position

decodeDouble::decodeDouble() : num() {}

decodeDouble::decodeDouble(double x) : num(x) {}

Fixed::Fixed() : l() {}

Fixed::Fixed(int x) : l((long long) x << POINT) {}

Fixed::Fixed(long long x) : l(x) {}

Fixed::Fixed(double x) : l() {
    decodeDouble xd(x);
    if (xd.field.mantissa == 0 && xd.field.exponent == 0) {
        l = 0;
        return;
    }
    unsigned long long xv = (1uLL << 52) | xd.field.mantissa;
    int shift = (int) (xd.field.exponent) - (1023 + 52 - POINT);
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
    xd.field.exponent = shift + (1023 + 52 - POINT);
    return xd.num;
}

Fixed Fixed::operator+(Fixed x) const {
    return Fixed(l + x.l);
}

Fixed Fixed::operator-(Fixed x) const {
    return Fixed(l - x.l);
}

Fixed Fixed::operator*(Fixed x) const {
    long long al = l < 0 ? -l : l;
    long long bl = x.l < 0 ? -x.l : x.l;
    long long ah = al >> 32;
    long long bh = bl >> 32;
    al &= 0xffffffffLL;
    bl &= 0xffffffffLL;
    long long r = ((al * bl) >> POINT) + ((al * bh + ah * bl) >> (POINT - 32)) + ((ah * bh) << (64 - POINT));
    return Fixed((l < 0) == (x.l < 0) ? r : -r);
}

Fixed Fixed::operator/(int x) const {
    return Fixed(l / x);
}

Fixed Fixed::operator-() const {
    return Fixed(-l);
}

bool Fixed::operator>(Fixed x) const {
    return operator-(x).l > 0;
}

void ECC::search(const std::vector<bool> &co, int nM, int nE) {
    std::vector<int> pE;
    for (int i = 0; i < co.size(); ++i)
        pE.push_back(i);
    search(co, nM, nE, pE);
}

void ECC::search(const std::vector<bool> &co, int nM, int nE, const std::vector<int> &pE) {
    code = co;
    posMissing.clear();
    posError = pE;
    numMissing = nM;
    numError = nE;
    hammingDistance = 0;
    res.clear();
    searchMissing(0);
}

void ECC::searchMissing(int i) {
    if (numMissing == 0)
        return searchError(0);
    for (; i <= code.size(); ++i) {
        for (auto &x: posError)
            if (x >= i) ++x;
        posMissing.push_back(i);
        code.insert(code.begin() + i, false); // insert 0
        --numMissing;
        ++hammingDistance;
        searchMissing(i + 1);

        code[i].flip(); // insert 1
        searchMissing(i + 1);

        for (auto &x: posError)
            if (x >= i) --x;
        posMissing.pop_back();
        code.erase(code.begin() + i);
        ++numMissing;
        --hammingDistance;
    }
}

void ECC::searchError(int i) {
    if (crcCheck<CRCL>(code))
        return res.push_back({code, posMissing, hammingDistance});
    if (numError == 0)
        return;
    for (; i < posError.size(); ++i) {
        code[posError[i]].flip();
        --numError;
        ++hammingDistance;
        searchError(i + 1);

        code[posError[i]].flip();
        ++numError;
        --hammingDistance;
    }
}

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

template<>
unsigned int crc<16>(const std::vector<bool> &source) {
    static unsigned char sourceString[20];
    int stringLength = ((int)source.size() - 1) / 8 + 1;
    for (int i = 0; i < stringLength; ++i) {
        unsigned q = 0;
        for (int j = 0; j < 8 && i * 8 + j < source.size(); ++j)
            q |= (int) source[i * 8 + j] << j;
        sourceString[i] = q;
    }
    boost::crc_16_type crc;
    crc.process_bytes(sourceString, stringLength);
    return crc.checksum();
}

template<>
unsigned int crc<32>(const std::vector<bool> &source) {
    static unsigned char sourceString[20];
    int stringLength = ((int)source.size() - 1) / 8 + 1;
    for (int i = 0; i < stringLength; ++i) {
        unsigned q = 0;
        for (int j = 0; j < 8 && i * 8 + j < source.size(); ++j)
            q |= (int) source[i * 8 + j] << j;
        sourceString[i] = q;
    }
    boost::crc_32_type crc;
    crc.process_bytes(sourceString, stringLength);
    return crc.checksum();
}

template<unsigned long long N>
bool crcCheck(std::vector<bool> source) {
    unsigned int check = 0;
    for (int i = 0; i < CRCL; ++i) {
        check |= *source.rbegin() << i;
        source.pop_back();
    }
    return check == crc<N>(source);
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