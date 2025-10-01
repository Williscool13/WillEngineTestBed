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

    std::vector<std::tuple<int, int, int> > tests = {
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

void Question_2_79()
{
    fmt::println("=== Question 2.79 ===");

    int w = sizeof(int) << 3;
    auto dividePower2 = [w](int x, int k) {
        int negMask = x >> (w - 1);
        int negativeBias = (1 << k) - 1;
        int bias = negMask & negativeBias;
        x += bias;
        return x >> k;
    };

    // mul3 div4
    // x*3 can overflow, let it.

    auto mul3div4 = [w, dividePower2](int x) {
        auto mult = (x << 1) + x;
        return dividePower2(mult, 2);
    };

    std::vector<std::tuple<int, int> > tests = {
        {0, 0},
        {8, 6},
        {-8, -6},
        {12, 9},
        {-12, -9},
        {1, 0},
        {-1, 0},
        {INT_MAX, 536870911}, // will overflow in mult
        {INT_MIN, -536870912}, // will overflow in mult
        {100, 75},
        {-100, -75},
    };

    for (auto [x, expected] : tests) {
        int result = mul3div4(x);
        fmt::println("mul3div4({}) -> {} (expected: {})", x, result, expected);
    }
}

void Question_2_80()
{
    fmt::println("=== Question 2.80 ===");

    int w = sizeof(int) << 3;

    // auto dividePower2 = [w](int x, int k) {
    //     int negMask = x >> (w - 1);
    //     int negativeBias = (1 << k) - 1;
    //     int bias = negMask & negativeBias;
    //     x += bias;
    //     return x >> k;
    // };
    //
    // auto saturatingAdd = [](int x, int y) {
    //     int sum = x + y;
    //     int sig_mask = INT_MIN;
    //     /*
    //      * if x > 0, y > 0 but sum < 0, it's a positive overflow
    //      * if x < 0, y < 0 but sum >= 0, it's a negative overflow
    //      */
    //     int pos_over = !(x & sig_mask) && !(y & sig_mask) && (sum & sig_mask);
    //     int neg_over = (x & sig_mask) && (y & sig_mask) && !(sum & sig_mask);
    //
    //     (pos_over && (sum = INT_MAX) || neg_over && (sum = INT_MIN));
    //
    //     return sum;
    // };
    //
    // auto threeFourths = [w, saturatingAdd, dividePower2](int x) {
    //     int r = saturatingAdd(x, x);
    //     r = saturatingAdd(r, x);
    //
    //     return dividePower2(r, 2);
    // }

    // mul3 div4
    // x*3 can overflow, DONT let it.
    auto threeFourths = [w](int x) {
        int negMask = x >> (w - 1);
        int f = x & ~0x3;
        int l = x & 0x3;

        // 3/4 of the w - 2 most significant bits. Right shift has no risk of rounding because last 2 bits are extracted into l
        int fd4 = f >> 2;
        int fd4m3 = (fd4 << 1) + fd4;

        int lm3 = (l << 1) + l;
        int bias = (1 << 2) - 1;
        lm3 += bias & negMask;
        int lm3d4 = lm3 >> 2;

        return fd4m3 + lm3d4;
    };

    std::vector<std::tuple<int, int> > tests = {
        {0, 0},
        {8, 6},
        {-8, -6},
        {12, 9},
        {-12, -9},
        {1, 0},
        {-1, 0},
        {INT_MAX, 1610612735}, // will overflow in mult
        {INT_MIN, -1610612736}, // will overflow in mult
        {100, 75},
        {-100, -75},
        {715827882, 536870911}, // right at overflow boundary (positive)
        {-715827883, -536870912} // right at overflow boundary (negative)
    };

    for (auto [x, expected] : tests) {
        int result = threeFourths(x);
        fmt::println("threeFourths({}) -> {} (expected: {})", x, result, expected);
    }

    // FAILED TO FIND SOLUTION
}

void Question_2_81()
{
    fmt::println("=== Question 2.81 ===");

    // 1^(w-k), 0^k
    auto expressionOne = [](int k) {
        return ~0x0 << k;
    };
    // 0^(w-k-j), 1^k, 0^j
    auto expressionTwo = [](int k, int j) {
        return ~(~0x0 << k) << j;
    };

    ShowBytes(expressionOne(4));
    ShowBytes(expressionTwo(4, 4));
}

void Question_2_82()
{
    fmt::println("=== Question 2.82 ===");

    int w = sizeof(int) << 3;

    // 32 bit, 2s complement, arithmetic right shift
    // int32_t
    // unsigned also 2 bits
    // uint32_t

    // create arbitrary values
    // int32_t x = random();
    // int32_t y = random();
    // uint32_t ux = (unsigned)x;
    // uint32_t uy = (unsigned)y;

    // will yield 1 or 0? If 0, give example why


    // A. (x < y) == (-x > -y)
    // false, if x == INT_MIN and y == 1
    fmt::println("2.82.A: {}", (INT_MIN < 1) == (-INT_MIN > -1));
    /*
     * B. ((x + y) << 4) + y - x == 17 * y + 15 * x;
     * Expands to
     * (x << 4 + y << 4 + y - x) == 17 * y + 15 * x;
     * (16x - x + 16y + y) = 15x + 17y
     * Add and mult are commutative and associative
     * true!
     *
     */

    /*
     * C. ~x + ~y + 1 == ~(x + y)
     * True because (~x = -x - 1) in all cases. Overflow affects both sides equally. The +1 is required to offset the (-1) in the conversion on the side that converts twice.
    */
    fmt::println("2.82.C: {}", ~1 + ~2 + 1 == ~(1 + 2));
    fmt::println("2.82.C: {}", ~(-1) + ~(-2) + 1 == ~((-1) + (-2)));

    /*
     * D. (ux - uy) == -(unsigned)(y - x)
     *
     * True
     * (ux - uy) == -(unsigned)(y - x)
     * -(ux - uy) == (unsigned)(y - x)
     * uy - ux == unsigned(y - x)
     * uy - ux == uy - ux
     */


    /*
     * E. ((x >> 2) << 2) <= x
     * this is just rounded down? True
     * >> 2 and << 2 like that removes the last 2 bits (always 0), so the resulting value is always smaller than or equal to the original value.
     */
}


void Question_2_83()
{
    fmt::println("=== Question 2.83 ===");

    int k = 3;
    auto y = [](unsigned Y, int k) {
        return (float) Y / ((1 << k) - 1);
    };

    fmt::println("y: {} --> expected {}", y(0x3, 4), 1.0f / 5);
    fmt::println("y1: {} --> expected {}", y(0x5, 3), 5.0f / 7);
    fmt::println("y2: {} --> expected {}", y(0x6, 4), 2.0f / 5);
    fmt::println("y3: {} --> expected {}", y(0x13, 6), 19.0f / 63);
}

void Question_2_84()
{
    fmt::println("=== Question 2.84 ===");

    auto f2u = [](float x) {
        return *(unsigned*) &x;
    };

    auto floatLE = [f2u](float x, float y) {
        unsigned ux = f2u(x);
        unsigned uy = f2u(y);

        unsigned sx = ux >> 31;
        unsigned sy = uy >> 31;

        int bothZero = (ux << 1 == 0 && uy << 1 == 0);
        int xNegYPos = (sx && !sy);
        int xPositiveLarger = !sx && !sy && ux <= uy;
        int yPositiveLarger = sx && sy && ux >= uy;

        return bothZero || xNegYPos || xPositiveLarger || yPositiveLarger;
    };
}

void Question_2_85()
{
    fmt::println("=== Question 2.85 ===");

    int k = 8;
    int n = 23;
    auto reconstructFloat = [k, n](int E, int f) {
        auto exponent = E - ((1 << (k - 1)) - 1);
        double e2 = pow(2, exponent);
        auto leading = (double) (!!E);
        return e2 * (leading + f / (double) (1 << n));
    };

    std::vector<std::tuple<int, int, double> > tests = {
        {127, 0, 1.0},
        {128, 2097152, 2.5},
        {0, 0, 0.0},
    };

    for (auto [E, f, expected] : tests) {
        double result = reconstructFloat(E, f);
        fmt::println("generateFloat({}, {}) -> {} (expected: {})", E, f, result, expected);
    }


    // bias = (1 << (k - 1)) - 1
    // V = 2^(E - bias) * (!!E + f / 1 << n);

    // E = floor(log2(V)) + ((1 << (k-1)) - 1)
    // Normalized
    // f = (V / (2^(E - bias)) - 1) * 2^n
    // M = 1 + f / 1 << n
    // Denormalized
    // f = (V / (2^(1 - bias))) * 2^n
    // M = f / 1 << n

    auto printFloat = [](double V, int k, int n) {
        int bias = (1 << (k - 1)) - 1;
        int E = (int) floor(log2(V)) + bias;

        if (E > 0) {
            // Normalized
            double f = (V / pow(2, E - bias) - 1) * pow(2, n);
            double M = 1 + f / pow(2, n);
            fmt::println("V={}, k={}, n={} -> E={}, M={}, f={}", V, k, n, E, M, (int) f);
            return std::make_tuple(E, (int) f);
        }
        else {
            // Denormalized
            E = 0;
            double f = (V / pow(2, 1 - bias)) * pow(2, n);
            double M = f / pow(2, n);
            fmt::println("V={}, k={}, n={} -> E={}, M={}, f={}", V, k, n, E, M, (int) f);
            return std::make_tuple(E, (int) f);
        }
    };

    auto [E, f] = printFloat(7.0, k, n);
    double result = reconstructFloat(E, f);
    fmt::println("Reconstructed: {}", result);

    // A. 7.0
    // E =

    // B. largest odd integer that can be represented exactly
    {
        auto [E2, f2] = printFloat(pow(2, 24) - 1, k, n);
        double result2 = reconstructFloat(E2, f2);
        fmt::println("Reconstructed: {}", result2);
    } {
        auto [E2, f2] = printFloat(pow(2, 24) + 1, k, n);
        double result2 = reconstructFloat(E2, f2);
        fmt::println("Reconstructed: {}", result2);
    }
    // C. reciprocal of the smallest positive normalized value

    {
        // ...
    }
}


void Question_2_86()
{
    fmt::println("=== Question 2.86 ===");

    // Intel extended precision FP
    // 1 sign bit
    // k = 15
    // 1 integer bit - denotes the leading 1 (normalized vs denormalized)
    // n = 63

    // smallest positive denormalized (nonzero)
    // 0x00000000000000000001
    // all zero except first fraction
    // 2 ^ (1 - bias) * 1 / 2^n

    // smallest norm
    // 0x00018000000000000000
    // 3x zero for sign
    // 1 for +1 for the exponent, even with the new integer, still need to be 1 to be normalized
    // 2 ^ (1 - bias) * (1 + 1 / 2^n)

    // largest norm
    // 0x7FFEFFFFFFFFFFFFFFFF
    // 7 for unset sign bit
    // E because last exponent can't be set, if so it is inf
}

void Question_2_87()
{
    fmt::println("=== Question 2.87 ===");

    // half
    // 1 sign, k = 5, n = 10
    // bias = (2^5-1) - 1 = 16 - 1 = 15

    /*
     * -0
     * HEX = 0x1000
     * M = 0 + 0 / 1024
     * E = 2 ^ 1 - 15 (Denormalized) = 1 / 16384
     * V = -0
     * D = -0
     */

    /*
     * smallest value > 2
     * would be where (2^E) = 2, fraction = 0000000001
     * E = 1 = e - 15, e = 16 = 10000
     * 0 10000 0000000001
     * HEX = 0x4001
     * M = 1 + 1 / 1024
     * E = 2 ^ (16 - 15) = 2
     * V = 2 * (1025/1024)
     * D = 2.0019531
     */
    fmt::println("{}", 2 * (1025.0f / 1024.0f));

    /*
     * 512
     * E = log2(512) = 9 = e - 15, e = 15 + 9 = 24
     * fraction should be 0
     * 0 11000 0000000000
     * HEX = 0x6000
     * M = 1 + 0 / 1024
     * E = 2 ^ (24 - 15) = 9
     * V = 2^9 * 1024/1024 = 512
     * D = 512.0
     */

    /*
     * - infinity
     * E = max value
     * fraction should be 0s, otherwise NaN
     * 1 11111 0000000000
     * HEX = 0xFC00
     * M = 1 + 0 / 1024
     * E = 2 ^ (32 - 15) = 17
     * V = -infinity
     * D = -infinity
     */

    /*
     * number with hex representation 3BB0
     * HEX = 0x3BB0
     * 0 01110 1110110000
     * M = 1 + 512 + 256 + 128 + 32 + 16 = 944 / 1024
     * E = 2 ^ (14 - 15) = 2^-1 = 1/2
     * V = 1/2 * 1968/1024 = 123 / 128
     * D = 0.9609375
     */
    fmt::println("{}", (123.0f / 128.0f));
}


void Question_2_88()
{
    fmt::println("=== Question 2.88 ===");

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
    Question_2_79();
    Question_2_80();
    Question_2_81();
    Question_2_82();
    Question_2_83();
    Question_2_84();
    Question_2_85();
    Question_2_86();
    Question_2_87();

    return 0;
}
