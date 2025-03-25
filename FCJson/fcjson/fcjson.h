#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <cfloat>

// VS sets the execution character set to UTF-8
// Project Property Pages -> Configuration Properties -> C/C++ -> Command Line -> Additional Options (D)
// Add /execution-charset:utf-8 or /utf-8
//

// This compiler directive is obsolete in Visual Studio 2015 Update 2 and later.
// #pragma execution_character_set("utf-8")

// JSON specification: https://www.json.org/json-zh.html

#ifdef _UNICODE

#define __T(x)      L ## x
using _tstring = std::wstring;
using _tchar = wchar_t;
using _utchar = _tchar;

#else

#define __T(x)      x
using _tstring = std::string;
using _tchar = char;
using _utchar = unsigned char;

#endif

#define _T(x)       __T(x)
#define _TEXT(x)    __T(x)

#define FC_JSON_RETURN              "\n"
#define FC_JSON_FLOAT_FORMAT        "%.16g"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define FC_JSON_INT64_FORMAT        "%lld"
#define FC_JSON_UINT64_FORMAT       "%llu"
#else
#define FC_JSON_INT64_FORMAT        "%ld"
#define FC_JSON_UINT64_FORMAT       "%lu"
#endif

// JSON Parse / Dump
// FlameCyclone
namespace fcjson
{
    class json_value;
    using json_null = nullptr_t;
    using json_bool = bool;
    using json_int = int64_t;
    using json_uint = uint64_t;
    using json_float = double;
    using json_string = _tstring;
    using json_object = std::map<_tstring, json_value>;
    using json_array = std::vector<json_value>;
    using json_bin = std::vector<uint8_t>;

    // Exception
    class json_exception
    {
    public:
        json_exception(const _tstring& msg) :
            m_msg(msg)
        {
        }

        _tstring get_message() const
        {
            return m_msg;
        }

        _tstring get_text_pos() const
        {
            return m_msg;
        }

    private:
        _tstring    m_msg;      // Message
    };

    // JSON Data type
    enum json_type :uint8_t
    {
        json_type_null,              // null
        json_type_bool,              // true Or false
        json_type_int,               // 0 - 9223372036854775807 Or -1 - (9223372036854775807 - 1)
        json_type_uint,              // 0 - 18446744073709551615
        json_type_float,             // 3.141592653589793
        json_type_string,            // "FlameCyclone"
        json_type_object,            // [128,256,512,1204,"string",{"name":"FlameCyclone"}]
        json_type_array,             // {"name":"FlameCyclone"}
        json_type_bin,               // binary data
    };

    // JSON Bin Tpye
    enum json_raw_type :uint8_t
    {
        raw_null            = 0x10, // Null
        raw_false           = 0x20, // Boolean false
        raw_true            = 0x30, // Boolean true

        raw_int8            = 0x40, // Signed int, value: [-128,-1]                 and [0,127]
        raw_int16           = 0x41, // Signed int, value: [-32768,-1]               and [0,32767]
        raw_int32           = 0x42, // Signed int, value: [-2147483648,-1]          and [0,2147483647]
        raw_int64           = 0x43, // Signed int, value: [-9223372036854775808,-1] and [0,9223372036854775807]

        raw_uint8           = 0x50, // Unsigned int, value: [0,255]
        raw_uint16          = 0x51, // Unsigned int, value: [0,65535]
        raw_uint32          = 0x52, // Unsigned int, value: [0,4294967295]
        raw_uint64          = 0x53, // Unsigned int, value: [0,18446744073709551615]

        raw_float           = 0x60, // Float, value: [2.2250738585072014e-308, 1.7976931348623158e+308]

        raw_string_empty    = 0xA0, // Empty string
        raw_string8         = 0xA1, // String, length: [0,255]
        raw_string16        = 0xA2, // String, length: [0,65535]
        raw_string32        = 0xA3, // String, length: [0,4294967295]

        raw_object_empty    = 0xB0, // Empty object
        raw_object_beg      = 0xB1, // Object Start
        raw_object_end      = 0xBF, // Object End

        raw_array_empty     = 0xC0, // Empty array
        raw_array_beg       = 0xC1, // Array Start
        raw_array_end       = 0xCF, // Array End

        raw_bin_empty       = 0xE0, // Empty binary
        raw_bin8            = 0xE1, // binary, length: [0,255]
        raw_bin16           = 0xE2, // binary, length: [0,65535]
        raw_bin32           = 0xE3, // binary, length: [0,4294967295]
    };

    // JSON Character Encoding
    enum json_encoding :uint8_t
    {
        json_encoding_auto,              // Auto
        json_encoding_utf8,              // Utf8
        json_encoding_utf16,             // Utf16
    };

    // JSON value class
    class json_value
    {
    public:

        // Constructor
        json_value(json_type type = json_type::json_type_object);
        json_value(nullptr_t);
        json_value(json_bool val);
        json_value(int32_t val);
        json_value(uint32_t val);
        json_value(int64_t val);
        json_value(uint64_t val);
        json_value(json_float val);
        json_value(const _tchar* val);
        json_value(const json_string& r);
        json_value(const json_object& r);
        json_value(const json_array& r);
        json_value(const json_bin& r);
        json_value(const json_value& r);
        json_value(json_value&& r) noexcept;
        json_value(json_string&& r);
        json_value(json_object&& r);
        json_value(json_array&& r);
        json_value(json_bin&& r);

        // Operator Overloading
        json_value& operator = (nullptr_t);
        json_value& operator = (json_type type);
        json_value& operator = (json_bool val);
        json_value& operator = (int32_t val);
        json_value& operator = (uint32_t val);
        json_value& operator = (int64_t val);
        json_value& operator = (uint64_t val);
        json_value& operator = (json_float val);
        json_value& operator = (const _tchar* val);
        json_value& operator = (const json_string& r);
        json_value& operator = (const json_object& r);
        json_value& operator = (const json_array& r);
        json_value& operator = (const json_value& r);
        json_value& operator = (json_string&& r);
        json_value& operator = (json_object&& r);
        json_value& operator = (json_array&& r);
        json_value& operator = (json_value&& r) noexcept;

        // [] Overloading, Accessing a non-existent index will create a new sub-item
        json_value& operator [] (const _tstring& val_name) noexcept;
        json_value& operator [] (size_t index) noexcept;

        ~json_value();

        // Type checking
        bool is_null() const;
        bool is_bool() const;
        bool is_int() const;
        bool is_float() const;
        bool is_number() const;
        bool is_string() const;
        bool is_object() const;
        bool is_array() const;
        bool is_bin() const;

        // Get data
        json_bool as_bool() const;
        json_int as_int() const;
        json_uint as_uint() const;
        json_float as_float() const;
        json_float as_number() const;
        json_string& as_string() const;
        json_object& as_object() const;
        json_array& as_array() const;
        json_bin& as_bin() const;

        // Parse
        bool parse(const _tstring& text);
        bool parse_from_file(const _tstring& file_path);
        bool parse_from_binary(uint8_t* data, size_t size);
        bool parse_from_binary_file(const _tstring& file_path);

        // Dump
        _tstring dump(int indent = 0, bool flag_escape = false) const;
        bool dump_to_file(const _tstring& file_path, int indent = 0, bool flag_escape = false, json_encoding enc = json_encoding::json_encoding_auto);
        std::vector<uint8_t> dump_to_binary();
        bool dump_to_binary_file(const _tstring& strPath);

        // Others
        size_t count() const;
        bool is_value(const _tstring& name = _T("")) const;
        bool remove(const _tstring& name);
        bool remove(const size_t index);
        json_type type() const;
        _tstring type_name() const;
        void clear();

#ifdef _WIN32

        // ANSI string
        static std::wstring AStrToWStr(const std::string& str);
        static std::string AStrToU8Str(const std::string& str);
        static _tstring AStrToTStr(const std::string& str);

        // Wide string
        static std::string WStrToU8Str(const std::wstring& str);
        static std::string WStrToAStr(const std::wstring& str);
        static _tstring WStrToTStr(const std::wstring& str);

        // UTF-8 string
        static std::wstring U8StrToWStr(const std::string& str);
        static std::string U8StrToAStr(const std::string& str);
        static _tstring U8StrToTStr(const std::string& str);

        // Text string
        static std::string TStrToAStr(const _tstring& str);
        static std::wstring TStrToWStr(const _tstring& str);
        static std::string TStrToU8Str(const _tstring& str);

#endif

    private:

        static json_value& _get_none_value();

        // Reset type
        inline void _reset_type(json_type type);

        // Parse
        bool _parse_number(const _tchar* data_ptr, json_value& val, const _tchar** end_ptr);
        bool _parse_unicode(const _tchar* data_ptr, _tstring& val, const _tchar** end_ptr);
        bool _parse_string(const _tchar* data_ptr, _tstring& val, const _tchar** end_ptr);
        bool _parse_object(const _tchar* data_ptr, json_value& val, const _tchar** end_ptr);
        bool _parse_array(const _tchar* data_ptr, json_value& val, const _tchar** end_ptr);
        bool _parse_value(const _tchar* data_ptr, json_value& val, const _tchar** end_ptr);
        bool _parse(const _tchar* data_ptr, json_value& val, const _tchar** end_ptr);

        // Dump
        void _dump_int(_tstring& append_buf, int64_t val) const;
        void _dump_uint(_tstring& append_buf, uint64_t val) const;
        void _dump_float(_tstring& append_buf, double val) const;
        void _dump_string(_tstring& append_buf, const _tstring& text, bool flag_escape) const;
        void _dump_object(_tstring& append_buf, std::vector<_tstring>& indent_text, int depth, int indent, bool flag_escape) const;
        void _dump_array(_tstring& append_buf, std::vector<_tstring>& indent_text, int depth, int indent, bool flag_escape) const;
        void _dump(_tstring& append_buf, std::vector<_tstring>& indent_text, int depth, int indent, bool flag_escape) const;

        // Dump raw
        void _dump_raw_int(std::vector<uint8_t>& append_buf, int64_t val) const;
        void _dump_raw_uint(std::vector<uint8_t>& append_buf, uint64_t val) const;
        void _dump_raw_float(std::vector<uint8_t>& append_buf, double val) const;
        void _dump_raw_string(std::vector<uint8_t>& append_buf, const std::string& text) const;
        void _dump_raw_object(std::vector<uint8_t>& append_buf, const json_object& object) const;
        void _dump_raw_array(std::vector<uint8_t>& append_buf, const json_array& arrry) const;
        void _dump_raw_bin(std::vector<uint8_t>& append_buf, const json_bin& arrry) const;
        void _dump_raw(std::vector<uint8_t>& append_buf) const;

        // Parse raw
        bool _parse_raw_string(const uint8_t* data_ptr, const uint8_t** end_ptr, json_value& val);
        bool _parse_raw_object(const uint8_t* data_ptr, const uint8_t** end_ptr, json_value& val);
        bool _parse_raw_array(const uint8_t* data_ptr, const uint8_t** end_ptr, json_value& val);
        bool _parse_raw_bin(const uint8_t* data_ptr, const uint8_t** end_ptr, json_value& val);
        bool _parse_raw(const uint8_t* data_ptr, const uint8_t** end_ptr, json_value& val);

    private:

        // JSON data
        union json_data
        {
            json_bool    _bool;             // bool
            json_int     _int;              // int64_t
            json_uint    _uint;             // uint64_t
            json_float   _float;            // double
            json_string* _string_ptr;       // std::string
            json_object* _object_ptr;       // std::map
            json_array*  _array_ptr;        // std::vector
            json_bin*    _raw_ptr;          // std::vector<uint8_t>
        }m_data;                            //

        json_type    m_type;                // Data type
    };
}