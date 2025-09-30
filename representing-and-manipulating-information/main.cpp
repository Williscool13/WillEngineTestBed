#include <cassert>
#include <vector>
#include <fmt/format.h>


typedef unsigned char* BytePointer;

template<typename T>
void ShowBytes(const T& value)
{
    const unsigned char* start = reinterpret_cast<const unsigned char*>(&value);
    for (size_t i = 0; i < sizeof(T); i++) {
        fmt::print("{:02x} ", start[i]);
    }
    fmt::print("\n");
}

void Question_2_58()
{
    fmt::println("=== Question 2.58 ===");
    constexpr unsigned one = 1;
    const auto* start = reinterpret_cast<const unsigned char*>(&one);
    if (start[0] == 1) {
        fmt::println("Is little endian machine");
    }
    else {
        fmt::println("Is big endian machine");
    }
}

void Question_2_59()
{
    fmt::println("=== Question 2.59 ===");
    constexpr unsigned x = 0x89ABCDEF;
    constexpr unsigned y = 0x76543210;

    ShowBytes((x & 0x000000FF) + (y & 0xFFFFFF00));
}

void Question_2_60()
{
    fmt::println("=== Question 2.60 ===");

    auto lambda = [](unsigned value, const size_t index, const unsigned char newChar) {
        if (index < sizeof(unsigned)) {
            fmt::print("Changing byte {} to 0x{:02x}\n", index, newChar);
            fmt::print("Old: ");
            ShowBytes(value);
            auto* start = reinterpret_cast<unsigned char*>(&value);
            start[index] = newChar;
            fmt::print("New: ");
            ShowBytes(value);
        }
    };

    lambda(0x12345678, 2, 0xAB);
    lambda(0x12345678, 0, 0xAB);
}

void Question_2_61()
{
    fmt::println("=== Question 2.61 ===");

    std::vector<int> tests = {0x00000000, (int) 0xFFFFFFFF, 0x12345678, (int) 0xFF000000};


    auto anyBitEqualsOne = [](int x) {
        return !!x;
    };

    auto anyBitEqualsZero = [](int x) {
        return !!~x;
    };

    auto anyLeastSignificantEqualsOne = [](int x) {
        return !!(x & 0xFF);
    };

    auto anyMostSignificantEqualsZero = [](int x) {
        size_t w = sizeof(int) << 3;
        size_t shift = w - 8;
        int mostSignificant = (x >> shift) & 0xFF;
        int inverseMostSignificant = (~mostSignificant) & 0xFF;

        // does inverted most significant have any 1s (if original has any 0s, there would be at least 1x 1s, so it should return true)
        return !!inverseMostSignificant;
    };

    for (auto x : tests) {
        fmt::println("0x{:08X}: {}, {}, {}, {}", x,
                     anyBitEqualsOne(x), anyBitEqualsZero(x),
                     anyLeastSignificantEqualsOne(x), anyMostSignificantEqualsZero(x));
    }
}

void Question_2_62()
{
    fmt::println("=== Question 2.62 ===");

    constexpr size_t w = sizeof(int) << 3;
    int a = INT_MIN;
    if (a >> (w - 1) == ~0x00) {
        fmt::println("Int shifts ARE arithmetic");
    }
    else {
        fmt::println("Int shifts ARE logical");
    }
}

void Question_2_63()
{
    fmt::println("=== Question 2.63 ===");

    constexpr size_t w = sizeof(int) << 3;

    auto srl = [w](unsigned x, int k) {
        unsigned xsra = (int) x >> k;

        int mask = (int) -1 << (w - k);
        return xsra & ~mask;
    };

    std::vector<std::pair<unsigned, int> > _tests = {
        {0x80000000u, 4}, // MSB set, shift by 4
        {0x12345678u, 8}, // mixed bits, shift by 8
        {0xFFFFFFFFu, 1}, // all 1s, shift by 1
        {0x7FFFFFFFu, 16} // no MSB, shift by 16
    };

    for (auto [x, k] : _tests) {
        fmt::println("srl(0x{:08X}, {}) = 0x{:08X}",
                     x, k, srl(x, k));
    }

    auto sra = [w](int x, int k) {
        int xsrl = (unsigned) x >> k;

        int mask = (int) -1 << (w - k);
        //let mask remain unchanged when the first bit of x is 1, otherwise 0.
        int m = 1 << (w - 1);
        // x & m checks if the sign bit in x is 1/0. If 0, -1 will underflow into 0xff... If 1, - 1 will set it to 0.
        mask &= !(x & m) - 1;
        return xsrl | mask;
    };

    std::vector<std::pair<int, int> > tests = {
        {0x80000000, 4}, // negative number, shift by 4
        {0x12345678, 8}, // positive number, shift by 8
        {0xFFFFFFFF, 1}, // all 1s (-1), shift by 1
        {0x7FFFFFFF, 16} // max positive, shift by 16
    };

    for (auto [x, k] : tests) {
        fmt::println("sra(0x{:08X}, {}) = 0x{:08X}",
                     static_cast<uint32_t>(x), k,
                     static_cast<uint32_t>(sra(x, k)));
    }
    // FAILED TO FIND SOLUTION
}


void Question_2_64()
{
    fmt::println("=== Question 2.64 ===");

    int w = 32;
    auto anyOddOne = [w](unsigned x) {
        return (int) !!(x & 0xAAAAAAAA);
    };

    std::vector<unsigned> tests = {
        0x00000000u, // no bits set
        0xAAAAAAAAu, // all odd bits set
        0x55555555u, // all even bits set
        0xFFFFFFFFu // all bits set
    };

    for (auto x : tests) {
        fmt::println("anyOddOne(0x{:08X}) = {}", x, anyOddOne(x));
    }
}

void Question_2_65()
{
    fmt::println("=== Question 2.65 ===");

    int w = 32;
    auto oddOnes = [w](unsigned x) {
        x ^= x >> 16;
        x ^= x >> 8;
        x ^= x >> 4;
        x ^= x >> 2;
        x ^= x >> 1;
        x &= 0x1;
        return x;
    };

    fmt::println("oddOnes (0x10101011): {}", oddOnes(0x10101011));
    fmt::println("oddOnes (0x01010101): {}", oddOnes(0x01010101));
    // FAILED TO FIND SOLUTION
}

void Question_2_66()
{
    fmt::println("=== Question 2.66 ===");

    int w = 32;
    auto leftmostOne = [w](unsigned x) {
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        x ^= x >> 1;
        return x;
    };

    std::vector<unsigned> tests = {
        0xFF000000u,
        0x66000000u,
        0x00000001u,
        0x80000000u,
        0x00000000u
    };

    for (auto x : tests) {
        fmt::println("leftmostOne(0x{:08X}) = 0x{:08X}", x, leftmostOne(x));
    }
}

void Question_2_67()
{
    fmt::println("=== Question 2.67 ===");

    // The sample code spits out a warning because shifting larger than width is UB, when bits are filled in beyond the word length is different between devices.

    // check if machine is 32 bit int, yield 1 if true - 0 if false.
    auto intSizeIsAtLeast32 = []() {
        int setMsb = 1 << 31;
        int beyondMsb = setMsb << 1;
        return setMsb && !beyondMsb;
    };

    auto intSizeIs32On16Bit = []() {
        int setMsb = 1 << 15 << 15 << 1;
        int beyondMsb = setMsb << 1;

        return setMsb && !beyondMsb;
    };

    fmt::println("intSizeIsAtLeast32() = {}", intSizeIsAtLeast32());
    fmt::println("intSizeIs32On16Bit() = {}", intSizeIs32On16Bit());
}

void Question_2_68()
{
    fmt::println("=== Question 2.68 ===");


    // assumed to be 32, but can be any multiple of 8
    int w = 32;
    // mask with least significant n bits set to 1
    // assume 1 <= n <= w
    auto lowerOneMask = [w](int n) {
        unsigned m = UINT_MAX;

        int nOffset = n - 1;
        // in case n == w, we split it into 2 downshifts. Worst case 32 -> << 1 << 31
        int leastSignificant = 1;
        leastSignificant &= nOffset;
        nOffset ^= leastSignificant;
        m = ~(m << leastSignificant << (nOffset + 1));
        return m;
    };

    fmt::println("lowerOneMask(6)");
    ShowBytes(lowerOneMask(6));
    fmt::println("lowerOneMask(17)");
    ShowBytes(lowerOneMask(17));
    fmt::println("lowerOneMask(32)");
    ShowBytes(lowerOneMask(32));
}

void Question_2_69()
{
    fmt::println("=== Question 2.69 ===");

    // 0 <= n < w
    // e.g. x = 0x12345678
    // n = 4 -> 0x23456781
    int w = sizeof(int) << 3;
    auto rotateLeft = [w](unsigned x, int n) {
        int right = (w - n) & (w - 1);
        unsigned a = x >> right;
        unsigned b = x << n;
        unsigned c = a | b;
        return c;
    };

    fmt::println("rotateLeft(0x12345678u, 4)");
    ShowBytes(rotateLeft(0x12345678u, 4));
    fmt::println("rotateLeft(0x12345678u, 20)");
    ShowBytes(rotateLeft(0x12345678u, 20));
    fmt::println("rotateLeft(0x12345678u, 0)");
    ShowBytes(rotateLeft(0x12345678u, 0));
}

void Question_2_70()
{
    fmt::println("=== Question 2.70 ===");

    int w = 32;
    auto fitBits = [w](int x, int n) {
        int shiftSize = (w - n);
        unsigned shiftedX = x << shiftSize >> shiftSize;
        return shiftedX == x;
    };
    std::vector<std::pair<int, int> > tests = {
        {0x00000005, 4}, // 5 fits in 4 bits signed (-8 to 7)
        {0x0000000F, 4}, // 15 doesn't fit in 4 bits signed
        {0xFFFFFFF8, 4}, // -8 fits in 4 bits signed
        {0xFFFFFFF7, 4} // -9 doesn't fit in 4 bits signed
    };

    for (auto [x, n] : tests) {
        fmt::println("fitsInNBits(0x{:08X}, {}) = {}",
                     static_cast<uint32_t>(x), n, fitBits(x, n));
    }
}

void Question_2_71()
{
    fmt::println("=== Question 2.71 ===");

    typedef unsigned packed_t;

    // Doesn't correctly extract negative numbers (1. Arithmetic shift, 2. Signed bit and preceding bits aren't flipped)
    // auto xbyte = [](packed_t word, int byteNum) {
    //     return (word >> (byteNum << 3)) & 0xFF;
    // };

    auto xbyte = [](packed_t word, int byteNum) {
        int max = 3;
        int leftShifted = (int) word << ((max - byteNum) << 3);
        int rightShifted = leftShifted >> (max << 3);
        return rightShifted;
    };

    std::vector<std::pair<packed_t, int> > tests = {
        {0x12345678, 0}, // Extract byte 0 (0x78 = 120, positive)
        {0x12345678, 2}, // Extract byte 2 (0x34 = 52, positive)
        {0x123456FF, 0}, // Extract byte 0 (0xFF = -1, negative)
        {0x80ABCDEF, 3} // Extract byte 3 (0x80 = -128, negative)
    };

    for (auto [word, byteNum] : tests) {
        fmt::println("xbyte(0x{:08X}, {}) = {}", word, byteNum, xbyte(word, byteNum));
    }
}

void Question_2_72()
{
    fmt::println("=== Question 2.72 ===");

    // Fails because maxBytes - sizeof(val) implicitly converts it to unsigned (size_t) and expression will always evaluate to true
    // auto copyInt = [](int val, void *buf, int maxBytes) {
    //     if (maxBytes - sizeof(val) >= 0) {
    //         memcpy(buf, (void*)&val, sizeof(val));
    //     }
    // };

    auto copyInt = [](int val, void* buf, int maxBytes) {
        if (maxBytes - (int) sizeof(val) >= 0) {
            memcpy(buf, (void*) &val, sizeof(val));
            return true;
        }
        return false;
    };

    std::vector<std::tuple<int, int> > tests = {
        {42, 4}, // sufficient space
        {100, 4}, // exact space (assuming sizeof(int) = 4)
        {200, 3}, // insufficient space
        {50, 0}, // zero bytes
        {75, -1}, // negative maxBytes (critical test)
        {123, -100}, // large negative maxBytes
        {999, 8}, // more than enough space
        {-50, 2}, // insufficient with negative value
    };

    int buffer[1];
    for (auto [val, maxBytes] : tests) {
        bool can = copyInt(val, buffer, maxBytes);
        fmt::println("copyInt({}, maxBytes={}) -> {}", val, maxBytes, can);
    }
}

void Question_2_73()
{
    fmt::println("=== Question 2.73 ===");

    // return TMax if overflow, TMin if underflow. Without conditionals of course.
    // auto saturatingAdd = [](int x, int y) {
    //     int overFlow = x + y;
    //     // if positive, check for 1st digit, cast to int, downshift 31, do an |
    //     // if negative, check for lack of first digit. If no 1 in MSB, do an & on 0x80...
    // };
    auto saturatingAdd = [](int x, int y) {
        int sum = x + y;
        int sig_mask = INT_MIN;
        /*
         * if x > 0, y > 0 but sum < 0, it's a positive overflow
         * if x < 0, y < 0 but sum >= 0, it's a negative overflow
         */
        int pos_over = !(x & sig_mask) && !(y & sig_mask) && (sum & sig_mask);
        int neg_over = (x & sig_mask) && (y & sig_mask) && !(sum & sig_mask);

        (pos_over && (sum = INT_MAX) || neg_over && (sum = INT_MIN));

        return sum;
    };

    fmt::print("{}", saturatingAdd(-1, -1));
    // FAILED TO FIND SOLUTION
}

void Question_2_74()
{
    fmt::println("=== Question 2.74 ===");

    // can subtraction happen without overflow
    int w = 32;
    auto tsubOk = [w](int x, int y) {
        int s = x - y;
        return !!(((x | ~y | ~s) & (~x | y | s)) & INT_MIN);
    };

    std::vector<std::tuple<int, int, bool> > tests = {
        {0, 0, true}, // zero - zero
        {5, 3, true}, // positive - positive, no overflow
        {-5, -3, true}, // negative - negative, no overflow
        {10, -5, true}, // positive - negative, normal case
        {-10, 5, true}, // negative - positive, normal case
        {INT_MAX, 0, true}, // max - zero
        {INT_MIN, 0, true}, // min - zero
        {0, INT_MAX, true}, // zero - max
        {0, INT_MIN, false}, // zero - min (critical: 0 - INT_MIN overflows)
        {INT_MAX, -1, false}, // max - (-1) = max + 1 (overflow)
        {INT_MAX, INT_MIN, false}, // max - min (overflow)
        {INT_MIN, 1, false}, // min - 1 (overflow)
        {INT_MIN, INT_MAX, true}, // min - max = min + (-max) (no overflow)
        {-1, INT_MIN, true}, // -1 - min (no overflow)
        {INT_MIN, -1, true}, // min - (-1) = min + 1 (no overflow)
    };

    for (auto [x, y, expected] : tests) {
        bool result = tsubOk(x, y);
        fmt::println("tsub_ok({}, {}) -> {} (expected: {})", x, y, result, expected);
    }
    // FAILED TO FIND SOLUTION
}

void Question_2_75()
{
    fmt::println("=== Question 2.75 ===");

    auto signed_high_prod = [](int x, int y) {
        int64_t mul = (int64_t) x * y;
        return (int) (mul >> 32);
    };

    auto unsigned_high_prod = [signed_high_prod](unsigned x, unsigned y) {
        /* number in bits of unsigned (or int) */
        int w = sizeof(x) << 3;

        /* high bits in signed form */
        int shp = signed_high_prod(x, y);

        /* extend the most significant bits of x and y to fill the whole numbers */
        int e_x = (int) x >> (w - 1);
        int e_y = (int) y >> (w - 1);

        /*
        * From equations 2.6, and derivations of 2.18, we have:
        * unsigned_high_prod = signed_high_prod + sign_of_x * y + size_of_y * x
        *
        * Extends the signs of x and y, and use them with bitwise and to replace the multiplication
        * (fit into the rule on page 128)
        */
        return (shp + (e_x & y) + (e_y & x));
    };

    std::vector<std::tuple<unsigned, unsigned> > tests = {
        {0, 0}, // zero * zero
        {1, 1}, // small * small
        {0xFFFFFFFF, 0xFFFFFFFF}, // max * max (critical: both have sign bit set)
        {0x80000000, 0x80000000}, // min_signed * min_signed as unsigned
        {0xFFFFFFFF, 1}, // max * 1
        {0xFFFFFFFF, 2}, // max * 2
        {0x12345678, 0x9ABCDEF0}, // arbitrary values with high bit set in second
        {0x7FFFFFFF, 0x7FFFFFFF}, // max_signed * max_signed
        {0x80000000, 1}, // high bit set * 1
        {0x80000000, 2}, // high bit set * 2
        {0xAAAAAAAA, 0x55555555}, // alternating bit patterns
        {0x00000001, 0x80000000}, // small * large with high bit
        {0x7FFFFFFF, 0xFFFFFFFF}, // max_signed * max_unsigned
        {0x12345678, 0x12345678}, // same values
        {0xFFFFFFFF, 0x80000000}, // max * min_signed
    };

    for (auto [x, y] : tests) {
        unsigned result = unsigned_high_prod(x, y);
        fmt::println("unsigned_high_prod(0x{:08X}, 0x{:08X}) -> 0x{:08X}", x, y, result);
    }
    // FAILED TO FIND SOLUTION
}


void Question_2_76()
{
    fmt::println("=== Question 2.76 ===");

    auto new_calloc = [](size_t nmemb, size_t size) -> void* {
        if (nmemb == 0 || size == 0) {
            return nullptr;
        }

        // Cause overflow from multiplication can't be inverted
        size_t total = nmemb * size;
        if (nmemb == total / size) {
            void* pos = malloc(total);
            if (pos) {
                memset(pos, 0, total);
            }
            return pos;
        }

        return nullptr;
    };
}

void Question_2_77()
{
    fmt::println("=== Question 2.77 ===");

    // multiply with only - + and <<
    // K = 17, -7, 60, -112

    int x = 10;

    // K = 17
    int a = (x << 4) + x;

    // K = -7
    int _b = (x << 3) - x;
    int b = _b - _b - _b;
    // int b = x - (x << 3); correct answer

    // K = 60
    int c = (x << 6) - (x << 2);

    // K = -112
    int _d = x << 7;
    int d = (_d - _d - _d) + (x << 4);
    // int d =  (x << 4) - (x << 7);

    fmt::println("multiplyConst() x = {}", x);
    fmt::println("multiplyConst(17) = {}", a);
    fmt::println("multiplyConst(-7) = {}", b);
    fmt::println("multiplyConst(60) = {}", c);
    fmt::println("multiplyConst(-112) = {}", d);
}

void Question_2_78()
{
    fmt::println("=== Question 2.78 ===");

    auto dividePower2 = [](int x, int k) {
        int w = sizeof(x) << 3;
        int signMask = x >> (w - 1);
        int bias = (1 << k) - 1;
        int maskedBias = signMask & bias;
        return (x + maskedBias) >> k;
    };

    std::vector<std::tuple<int, int, int>> tests = {
        {15, 2, 3},
        {-15, 2, -3},
        {17, 3, 2},
        {-17, 3, -2},
    };

    for (auto [x, k, expected] : tests) {
        int result = dividePower2(x, k);
        fmt::println("dividePower2({}, {}) -> {} (expected: {})", x, k, result, expected);
    }
}

int main()
{
    fmt::println("=== Representing and Manipulation Information ===");

    fmt::print("(uint32_t) 1 as bytes: ");
    ShowBytes(1);
    fmt::print("(uint32_t) 255 as bytes: ");
    ShowBytes(255);
    fmt::print("(uint32_t) UINT32_MAX as bytes: ");
    const uint32_t max = UINT32_MAX;
    ShowBytes(max);


    fmt::print("(uint16_t) 1 as bytes: ");
    ShowBytes(static_cast<uint16_t>(1));

    fmt::print("(uint64_t) 1 as bytes: ");
    ShowBytes(static_cast<uint64_t>(1));


    fmt::print("(double) 1 as bytes: ");
    ShowBytes(7.5);

    Question_2_58();
    Question_2_59();
    Question_2_60();
    Question_2_61();
    Question_2_62();
    Question_2_63();
    Question_2_64();
    Question_2_65();
    Question_2_66();
    Question_2_67();
    Question_2_68();
    Question_2_69();
    Question_2_70();
    Question_2_71();
    Question_2_72();
    Question_2_73();
    Question_2_74();
    Question_2_75();
    Question_2_76();
    Question_2_77();
    Question_2_78();

    return 0;
}
