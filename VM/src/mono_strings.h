#include <string>

using CodepointString = std::basic_string<char32_t>;

CodepointString utf8str_to_codepoints(const char *utf8str, size_t len);
std::string codepoints_to_utf8str(const CodepointString& utf32str, size_t len);

std::string to_upper_mono(const char *in, size_t len);
std::string to_lower_mono(const char *in, size_t len);
