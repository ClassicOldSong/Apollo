#include <string>

/**
 * @brief Convert a UTF-8 string into a UTF-16 wide string.
 * @param string The UTF-8 string.
 * @return The converted UTF-16 wide string.
 */
std::wstring from_utf8(const std::string_view &string);

/**
 * @brief Convert a UTF-16 wide string into a UTF-8 string.
 * @param string The UTF-16 wide string.
 * @return The converted UTF-8 string.
 */
std::string to_utf8(const std::wstring_view &string);