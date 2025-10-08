#include "std.hpp"
#include <sstream>
#include <iomanip>
#include <Windows.h>

int measure_codepoint(char chr) {
    if ((chr & 0x80) == 0x00) return 1;

    int len = 0;
    while (((chr >> (7 - len)) & 0x01) == 0x01) {
        len++;
        if (len > 8) return 8;
    }
    return len;
}

int decode_character(const char* buf, int index) {
    int codepointLen = measure_codepoint(buf[index]);

    if (codepointLen == 1) return buf[index];
    else {
        int newChar = buf[index] & (0x7f >> codepointLen);
        for (int j = 1; j < codepointLen; j++) {
            newChar = (newChar << 6) | (buf[index + j] & 0x3f);
        }
        return newChar;
    }
}

int encode_character(int chr, char* result) {
    if ((chr & 0x7f) == chr) {
        if (result != nullptr) { result[0] = chr; }
        return 1;
    }

    int len = 1;

    while ((chr & (~0x3f)) != 0x00) {
        if (result != nullptr) { result[len - 1] = 0x80 | (chr & 0x3f); }
        chr >>= 6;
        len++;
    }

    char firstByte = 0x00;
    for (int i = 0; i < len; i++) {
        firstByte |= (0x1 << (7 - i));
    }

    if (((firstByte | (0x1 << (7 - len))) & chr) == 0x00) {
        firstByte = firstByte | chr;
        if (result != nullptr) { result[len - 1] = firstByte; }
    }
    else {
        if (result != nullptr) { result[len - 1] = 0x80 | (chr & 0x3f); }
        chr >>= 6;
        firstByte = (firstByte | (0x1 << (7 - len))) | chr;
        len++;
        if (result != nullptr) { result[len - 1] = firstByte; }
    }

    if (result != nullptr) {
        for (int i = 0; i < len / 2; i++) {
            char b = result[i];
            result[i] = result[len - 1 - i];
            result[len - 1 - i] = b;
        }
    }

    return len;
}

bool isgraph_safe(int chr) {
    if (chr > 127) { return true; }
    return std::isgraph(chr);
}

_STDLIB_BEGIN

_ALWAYS_INLINE
ZStr _STDLIB(create_string__)(const char* raw) {
    return new std::string(raw);
}

_ALWAYS_INLINE
void _STDLIB(release_string__)(ZStr str) {
    // danger! do not imitate
    delete const_cast<std::string*>(str);
}

_ALWAYS_INLINE
ZStr _STDLIB(int_to_string__)(int raw) {
    return new std::string(std::to_string(raw));
}

_ALWAYS_INLINE
ZStr _STDLIB(float_to_string__)(float raw) {
    return new std::string(std::to_string(raw));
}

_ALWAYS_INLINE
ZStr _STDLIB(Concat)(ZStr str, ZStr b) {
    return new std::string(*str + *b);
}

ZStr _STDLIB(Replace)(ZStr str, ZStr pattern, ZStr newpat) {
    std::string* result = new std::string();
    result->reserve(str->size());

    size_t pos = 0, prev_pos = 0;

    while ((pos = str->find(*pattern, pos)) != std::string::npos) {
        result->append(str->substr(prev_pos, pos - prev_pos));
        result->append(*newpat);
        pos += pattern->size();
        prev_pos = pos;
    }

    result->append(str->substr(prev_pos));
    return result;
}

ZStr _STDLIB(String)(ZStr str, int n) {
    std::string* result = new std::string();
    while (n-- > 0) result->append(*str);
    return result;
}

int _STDLIB(Len)(ZStr str) {
    int utf8Len = 0;
    for (int i = 0; i < str->size();) {
        utf8Len++;
        i += measure_codepoint(str->at(i));
    }
    return utf8Len;
}

ZStr _STDLIB(Substr)(ZStr str, int start, int length) {
    int utf8Index = 0;
    int bytesStart = str->size();
    int bytesLength = str->size();
    for (int i = 0; i < str->size();) {
        if (start == utf8Index) {
            bytesStart = i;
        }
        if ((start + length) == utf8Index) {
            bytesLength = i - bytesStart;
            break;
        }
        utf8Index++;
        i += measure_codepoint(str->at(i));
    }
    return new std::string(str->substr(bytesStart, bytesLength));
}

_ALWAYS_INLINE
ZStr _STDLIB(Left)(ZStr str, int n) {
    return _STDLIB(Substr)(str, 0, n);
}

_ALWAYS_INLINE
ZStr _STDLIB(Right)(ZStr str, int n) {
    n = _STDLIB(Len)(str); if (n < 0) n = 0;
    return _STDLIB(Substr)(str, n, str->size());
}

_ALWAYS_INLINE
ZStr _STDLIB(Mid)(ZStr str, int start, int count) {
    if (start > str->size()) start = str->size();
    if (count >= 0) return _STDLIB(Substr)(str, start, count);
    return _STDLIB(Substr)(str, start, str->size());
}

ZStr _STDLIB(ConvertToUTF8)(ZStr str) {
    UINT nLen = MultiByteToWideChar(GetACP(), NULL, str->c_str(), -1, NULL, NULL);
    WCHAR* wszBuffer = new WCHAR[nLen + 1];
    nLen = MultiByteToWideChar(GetACP(), NULL, str->c_str(), -1, wszBuffer, nLen);
    wszBuffer[nLen] = 0;

    nLen = WideCharToMultiByte(CP_UTF8, NULL, wszBuffer, -1, NULL, NULL, NULL, NULL);
    CHAR* szBuffer = new CHAR[nLen + 1];
    nLen = WideCharToMultiByte(CP_UTF8, NULL, wszBuffer, -1, szBuffer, nLen, NULL, NULL);
    szBuffer[nLen] = 0;

    std::string* converted = new std::string(szBuffer);
    delete[] wszBuffer;
    delete[] szBuffer;
    return converted;
}

ZStr _STDLIB(ConvertToANSI)(ZStr str) {
    UINT nLen = MultiByteToWideChar(CP_UTF8, NULL, str->c_str(), -1, NULL, NULL);
    WCHAR* wszBuffer = new WCHAR[nLen + 1];
    nLen = MultiByteToWideChar(CP_UTF8, NULL, str->c_str(), -1, wszBuffer, nLen);
    wszBuffer[nLen] = 0;

    nLen = WideCharToMultiByte(GetACP(), NULL, wszBuffer, -1, NULL, NULL, NULL, NULL);
    CHAR* szBuffer = new CHAR[nLen + 1];
    nLen = WideCharToMultiByte(GetACP(), NULL, wszBuffer, -1, szBuffer, nLen, NULL, NULL);
    szBuffer[nLen] = 0;

    std::string* converted = new std::string(szBuffer);
    delete[] szBuffer;
    delete[] wszBuffer;
    return converted;
}

bool _STDLIB(IsValidUTF8String)(ZStr str) {
    for (size_t i = 0; i < str->size();) {
        int counts = measure_codepoint(str->at(i));
        if (counts > 4) return false;
        for (size_t j = 1; j < counts; j++)
        {
            if ((str->at(i + 1) & 0xC0) != 0x80) return false;
        }
        i += counts;
    }
    return true;
}

int _STDLIB(Instr)(ZStr str, ZStr in, int from) {
    from--;
    int utf8Index = 0;
    int bytesFrom = -1;
    for (int i = 0; i < str->size();) {
        if (from == utf8Index) {
            bytesFrom = i;
            break;
        }
        utf8Index++;
        i += measure_codepoint(str->at(i));
    }
    if (bytesFrom < 0) { return -1; }
    int bytesResult = str->find(*in, bytesFrom);
    int result = -1;
    for (int i = bytesFrom; i < str->size();) {
        if (bytesResult == i) {
            result = utf8Index;
            break;
        }
        utf8Index++;
        i += measure_codepoint(str->at(i));
    }
    return result < 0 ? 0 : result + 1;
}

ZStr _STDLIB(Upper)(ZStr str) {
    std::string* result = new std::string();
    for (int i = 0; i < str->size(); ++i) result->push_back(std::toupper(str->at(i)));
    return result;
}

ZStr _STDLIB(Lower)(ZStr str) {
    std::string* result = new std::string();
    for (int i = 0; i < str->size(); ++i) result->push_back(std::tolower(str->at(i)));
    return result;
}

ZStr _STDLIB(Trim)(ZStr str) {
    size_t start = 0;
    size_t end = str->size();

    while (start < end && !isgraph_safe(str->at(start))) start++;
    while (end > start && !isgraph_safe(str->at(end - 1))) end--;

    return new std::string(str->substr(start, end - start));
}

ZStr _STDLIB(LSet)(ZStr str, int n) {
    if (str->size() > n) return new std::string(str->substr(0, n));
    std::string* result = new std::string(*str);
    while (result->size() < n) result->push_back(' ');
    return result;
}

ZStr _STDLIB(RSet)(ZStr str, int n) {
    if (str->size() > n) return new std::string(str->substr(str->size() - n));
    std::string* result = new std::string(*str);
    while (result->size() < n) result->insert(result->begin(), ' ');
    return result;
}

ZStr _STDLIB(Chr)(int value) {
    return new std::string(1, (char)value);
}

ZStr _STDLIB(Hex)(int value) {
    static char buff[33];
    _itoa_s(value, buff, 16);
    return new std::string(buff);
}

ZStr _STDLIB(Bin)(int value) {
    static char buff[33];
    _itoa_s(value, buff, 2);
    return new std::string(buff);
}

int _STDLIB(Asc)(ZStr str) {
    return str->size() ? str->at(0) & 255 : -1;
}

ZStr _STDLIB(HighPrecisionFloatString)(float value) {
    std::ostringstream stream;
    stream << std::setprecision(20) << value;
    return new std::string(stream.str());
}

_STDLIB_END