#pragma once
#include <string.h>
#include <iostream>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

#ifndef PATTERN_DETECTOR_HPP
#define PATTERN_DETECTOR_HPP


namespace pattern
{
    uintptr_t text_addr;
    size_t text_size;
    bool bInited = false;

    void remove_spaces_and_format(char* str)
    {
        int count = 0;
        for (int i = 0; str[i]; i++)
        {
            if (str[i] == '?')
                str[count++] = '?';
            if (str[i] != ' ')
                str[count++] = str[i];
        }
        str[count] = '\0';
    }

    uint8_t hextobin(const char* str, uint8_t* bytes, size_t blen, uint8_t* wildcards)
    {
        uint8_t  pos;
        uint8_t  idx0;
        uint8_t  idx1;

        static const uint8_t hashmap[] =
        {
          0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // 01234567
          0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 89:;<=>?
          0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, // @ABCDEFG
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // HIJKLMNO
        };

        memset(bytes, 0, blen);
        for (pos = 0; ((pos < (blen * 2)) && (pos < strlen(str))); pos += 2)
        {
            idx0 = ((uint8_t)str[pos + 0] & 0x1F) ^ 0x10;
            idx1 = ((uint8_t)str[pos + 1] & 0x1F) ^ 0x10;
            bytes[pos / 2] = (uint8_t)(hashmap[idx0] << 4) | hashmap[idx1];
            if (idx0 == 0xf && idx1 == 0xf)
                wildcards[pos / 2] = 1;
            else
                wildcards[pos / 2] = 0;
        };

        return pos / 2;
    }

    //uint8_t* bytes_find(uint8_t* haystack, size_t haystackLen, uint8_t* needle, size_t needleLen) {
    //    size_t len = needleLen;
    //    size_t limit = haystackLen - len;
    //    for (size_t i = 0; i <= limit; i++) {
    //        size_t k = 0;
    //        for (; k < len; k++) {
    //            if ((needle[k] != haystack[i + k])) break;
    //        }
    //        if (k == len) return haystack + i;
    //    }
    //    return NULL;
    //}
    //
    //uint8_t* bytes_find(uint8_t* haystack, size_t haystackLen, uint8_t* needle, size_t needleLen, uint8_t* wildcards) {
    //    size_t len = needleLen;
    //    size_t limit = haystackLen - len;
    //    for (size_t i = 0; i <= limit; i++) {
    //        size_t k = 0;
    //        for (; k < len; k++) {
    //            if ((needle[k] != haystack[i + k]) && wildcards[k] != 1) break;
    //        }
    //        if (k == len) return haystack + i;
    //    }
    //    return NULL;
    //}

    uint8_t* bytes_find_nth(size_t count, uint8_t* haystack, size_t haystackLen, uint8_t* needle, size_t needleLen) {
        size_t n = 0;
        size_t len = needleLen;
        size_t limit = haystackLen - len;
        for (size_t i = 0; i <= limit; i++) {
            size_t k = 0;
            for (; k < len; k++) {
                if ((needle[k] != haystack[i + k])) break;
            }
            if (k == len)
            {
                if (n == count)
                    return haystack + i;
                else
                    n++;
            }
        }
        return NULL;
    }

    uint8_t* bytes_find_nth(size_t count, uint8_t* haystack, size_t haystackLen, uint8_t* needle, size_t needleLen, uint8_t* wildcards) {
        size_t n = 0;
        size_t len = needleLen;
        size_t limit = haystackLen - len;
        for (size_t i = 0; i <= limit; i++) {
            size_t k = 0;
            for (; k < len; k++) {
                if ((needle[k] != haystack[i + k]) && wildcards[k] != 1) break;
            }
            if (k == len)
            {
                if (n == count)
                    return haystack + i;
                else
                    n++;
            }
        }
        return NULL;
    }

    namespace range_pattern {
        uintptr_t get(size_t count, uintptr_t range_start, size_t range_size, uint8_t* buf, size_t size, int32_t offset)
        {
            uintptr_t result = (uintptr_t)bytes_find_nth(count, (uint8_t*)range_start, range_size, buf, size);

            if (result)
                return result + offset;
            else
                return 0;
        }

        uintptr_t get(size_t count, uintptr_t range_start, size_t range_size, const char* pattern_str, int32_t offset)
        {
            if (pattern_str[0] == '\0')
                return 0;
            char* str = (char*)malloc(strlen(pattern_str) + 1);
            strcpy(str, pattern_str);
            pattern_str = str;
            uint8_t* wc = (uint8_t*)malloc(strlen(pattern_str) + 1);
            remove_spaces_and_format((char*)pattern_str);
            size_t len = strlen(pattern_str);
            uint8_t* buf = (uint8_t*)malloc(len);
            uint8_t size = hextobin(pattern_str, buf, len, wc);
            uintptr_t result = (uintptr_t)bytes_find_nth(count, (uint8_t*)range_start, range_size, buf, size, wc);

            free(buf);
            free(wc);
            free(str);

            if (result)
                return result + offset;
            else
                return 0;
        }

        bool get_multiple(uintptr_t range_start, size_t range_size, const char* pattern_str, size_t max_count, std::vector<uintptr_t>* out)
        {
            bool bFoundOne = false;
            uintptr_t startAddr = pattern::text_addr;
            uintptr_t startSize = pattern::text_size;

            for (size_t i = 0; i < max_count; i++)
            {
                uintptr_t searchAddr = get(0, startAddr, startSize, pattern_str, 0);
                if (searchAddr)
                {
                    bFoundOne = true;
                    out->push_back(searchAddr);
                }
                else
                    break;

                startSize = startSize - ((searchAddr + 1) - startAddr);
                startAddr = searchAddr + 1;
            }

            return bFoundOne;
        }

        bool get_multiple(uintptr_t range_start, size_t range_size, uint8_t* buf, size_t size, size_t max_count, std::vector<uintptr_t>* out)
        {
            bool bFoundOne = false;
            uintptr_t startAddr = pattern::text_addr;
            uintptr_t startSize = pattern::text_size;

            for (size_t i = 0; i < max_count; i++)
            {
                uintptr_t searchAddr = get(0, startAddr, startSize, buf, size, 0);
                if (searchAddr)
                {
                    bFoundOne = true;
                    out->push_back(searchAddr);
                }
                else
                    break;

                startSize = startSize - ((searchAddr + 1) - startAddr);
                startAddr = searchAddr + 1;
            }

            return bFoundOne;
        }
    };

    void SetGameBaseAddress(uintptr_t addr, size_t size)
    {
        text_addr = addr;
        text_size = size;
    }

    inline uintptr_t get(size_t count, const char* pattern_str, int32_t offset = 0)
    {
        return range_pattern::get(count, text_addr, text_size, pattern_str, offset);
    }

    inline uintptr_t get_first(const char* pattern_str, int32_t offset = 0)
    {
        return range_pattern::get(0, text_addr, text_size, pattern_str, offset);
    }

    inline bool get_multiple(const char* pattern_str, size_t max_count, std::vector<uintptr_t>* out)
    {
        return range_pattern::get_multiple(text_addr, text_size, pattern_str, max_count, out);
    }

    inline uintptr_t get(size_t count, uint8_t* buf, size_t size, int32_t offset = 0)
    {
        return range_pattern::get(count, text_addr, text_size, buf, size, offset);
    }

    inline uintptr_t get_first(uint8_t* buf, size_t size, int32_t offset = 0)
    {
        return range_pattern::get(0, text_addr, text_size, buf, size, offset);
    }

    inline bool get_multiple(uint8_t* buf, size_t size, size_t max_count, std::vector<uintptr_t>* out)
    {
        return range_pattern::get_multiple(text_addr, text_size, buf, size, max_count, out);
    }

    namespace Win32
    {
        DWORD GetTextSectionSize(HMODULE hModule) 
        {
            // Get DOS header
            IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)hModule;

            // Check for the DOS signature
            if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) 
            {
                return 0;
            }

            // Get NT headers
            IMAGE_NT_HEADERS32* ntHeaders = (IMAGE_NT_HEADERS32*)((BYTE*)hModule + dosHeader->e_lfanew);

            // Check for the PE signature
            if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) 
            {
                return 0;
            }

            // Get the section headers
            IMAGE_SECTION_HEADER* sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);

            // Iterate through the sections to find the .text section
            for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) 
            {
                if (strcmp((char*)sectionHeader[i].Name, ".text") == 0) 
                {
                    bInited = true;
                    // Return the size of the .text section
                    return sectionHeader[i].SizeOfRawData;
                }
            }

            // .text section not found
            return 0;
        }

        void Init()
        {
            bInited = false;
            HMODULE hModule = GetModuleHandle(NULL);
            SetGameBaseAddress((uintptr_t)hModule, GetTextSectionSize(hModule));
        }

        bool bIsInited()
        {
            return bInited;
        }
    }
}
#endif