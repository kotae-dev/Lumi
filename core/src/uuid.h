#pragma once
#include <string>

// Generate a UUID v4 string into the provided buffer (must be at least 37 bytes).
void generate_uuid_v4(char* out);

// Generate a UUID v4 as a std::string.
std::string generate_uuid_v4_string();
