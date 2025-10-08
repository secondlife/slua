#pragma once

#include <cstdint>
#include <climits>
#include <string>
#include <vector>

class NumberFormatter {
public:
    static std::string NumberToString(const char *format, float value, bool neg_zero);
private:
    const int HundredMillion = 100000000;
    const int64_t SeventeenDigitsThreshold = 10000000000000000LL;

    const int DoubleBitsExponentShift = 52;
    const int DoubleBitsExponentMask = 0x7ff;
    const int64_t DoubleBitsMantissaMask = 0xfffffffffffffLL;

    bool _supportNegZero = true;
    bool _isNaN = false;
    bool _infinity = false;
    bool _positive = false;
    char _specifier = 0;

    int _precision = 0;
    int _defPrecision = 0;

    int _digitsLen = 0;
    int _offset = 0; // Represent the first digit offset.
    int _decPointPos = 0;

    // The following fields are a hexadeimal representation of the digits.
    // For instance _val = 0x234 represents the digits '2', '3', '4'.
    uint32_t _val1 = 0; // Digits 0 - 7.
    uint32_t _val2 = 0; // Digits 8 - 15.
    uint32_t _val3 = 0; // Digits 16 - 23.
    uint32_t _val4 = 0; // Digits 23 - 31. Only needed for decimals.

    int _ind = 0;

    std::vector<char> _cbuf;

    [[nodiscard]] int IntegerDigits() const {
        return _decPointPos > 0 ? _decPointPos : 1;
    }

    void InitDecHexDigits(uint64_t value);

    static uint32_t FastToDecHex(int val);

    static uint32_t ToDecHex(int val);

    static int FastDecHexLen(int val);

    static int DecHexLen(uint32_t val);

    [[nodiscard]] int DecHexLen() const;

    static int ScaleOrder(int64_t hi);

    [[nodiscard]] int InitialFloatingPrecision() const;

    static int ParsePrecision(const char *format);

    void Init(const char* format);

    void Init(const char *format, double value, int defPrecision);

    void ResetCharBuf(int size);

    void Resize(int len);


    void Append(char c, int cnt);

    void Append(std::string s);

    bool RoundDecimal(int decimals);

    bool RoundBits(int shift);

    void RemoveTrailingZeros();

    void AddOneToDecHex();

    static uint32_t AddOneToDecHex(uint32_t val);

    [[nodiscard]] int CountTrailingZeros() const;

    static int CountTrailingZeros(uint32_t val);

    std::string FormatFixedPoint(int precision);

    void AppendIntegerString(int minLength);

    void AppendDecimalString(int precision);

    void AppendDigits(int start, int end);
};