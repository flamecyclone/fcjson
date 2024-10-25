#include "fcjson.h"
#include <cwctype>
#include <fstream>
#include <cstring>
#include <cstdio>

// UTF-8 encoding standard
// 
// 1Byte  U+0000000 - U+0000007F 0xxxxxxx
// 2Bytes U+0000080 - U+000007FF 110xxxxx 10xxxxxx
// 3Bytes U+0000800 - U+0000FFFF 1110xxxx 10xxxxxx 10xxxxxx
// 4Bytes U+0010000 - U+001FFFFF 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
// 5Bytes U+0200000 - U+03FFFFFF 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
// 6Bytes U+4000000 - U+7FFFFFFF 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx

// UTF-16 encoding standard
// 
//
// Basic Multilingual Plane (U+0000 - U+FFFF)
//
// Supplementary Planes (U+10000 - U+10FFFF)
// 1. Subtract 0x10000 from the code point to get a 20-bit surrogate value (0x00 - 0xFFFFF)
// 2. Add 0xD800 to the high 10 bits (range 0 - 0x3FF) to get the high surrogate (0xD800 - 0xDBFF)
// 3. Add 0xDC00 to the low 10 bits (range 0 - 0x3FF) to get the low surrogate (0xDC00 - 0xDFFF)

namespace fcjson
{

#ifdef _UNICODE

#define _json_istxdigit          std::iswxdigit
#define _json_istdigit           std::iswdigit
#define _json_tcsncmp            std::wcsncmp
#define _json_tcsstr             std::wcsstr
#define _json_tcstod             std::wcstod
#define _json_tcstol             std::wcstol
#define _json_tcstoll            std::wcstoll
#define _json_tcstoull           std::wcstoull
#define _json_stprintf_s         _snwprintf
#define _json_tisgraph           iswgraph
#else

#define _json_istxdigit          std::iswxdigit
#define _json_istdigit           std::iswdigit
#define _json_tcsncmp            std::strncmp
#define _json_tcsstr             std::strstr
#define _json_tcstod             std::strtod
#define _json_tcstol             std::strtol
#define _json_tcstoll            std::strtoll
#define _json_tcstoull           std::strtoull
#define _json_stprintf_s         snprintf
#define _json_tisgraph           isgraph

#endif

    static std::string _get_utf8_text_for_code_point(uint32_t cp32);
    static bool _get_utf16_code_point(const _tchar* data_ptr, uint32_t* code_point_ptr, const _tchar** end_ptr);
    static bool _get_unicode_string(_tstring& append_str, const _tchar* data_ptr, const _tchar** end_ptr);
    static int32_t _utf8_to_utf16(const void* data_ptr, size_t size = -1, std::string* text_utf8_ptr = nullptr, std::wstring* text_utf16_ptr = nullptr);
    static int32_t _utf16_to_utf8(const void* data_ptr, size_t size = -1, std::string* text_utf8_ptr = nullptr, std::wstring* text_utf16_ptr = nullptr);
    inline const _tchar* _skip_whitespace(const _tchar* data_ptr);
    inline const _tchar* _skip_bom(const _tchar* data_ptr);
    static bool _skip_digit(const _tchar* data_ptr, const _tchar** end_ptr);
    
    inline const _tchar* _skip_whitespace(const _tchar* data_ptr)
    {
        while (_T('\0') != *data_ptr)
        {
            if (*data_ptr > _T(' '))
            {
                break;
            }

            data_ptr++;
        }

        return data_ptr;
    }

    inline const _tchar* _skip_bom(const _tchar* data_ptr)
    {
#ifdef _UNICODE

        while (0xFEFF == *data_ptr)
        {
            data_ptr++;
        }

#else
        while (nullptr != _json_tcsstr(data_ptr, "\xEF\xBB\xBF"))
        {
            data_ptr += 3;
        }

#endif
        return data_ptr;
    }

    json_value::json_value()
        :
        m_data{ 0 },
        m_type(json_type::json_type_null)
    {
    }

    json_value::json_value(json_type type) :
        m_data{ 0 },
        m_type(type)
    {
    }

    json_value::json_value(nullptr_t) : json_value()
    {
    }

    json_value::json_value(json_bool val)
    {
        m_type = json_type::json_type_bool;
        m_data._bool = val;
    }

    json_value::json_value(int32_t val)
    {
        m_type = json_type::json_type_int;
        m_data._int = val;
    }

    json_value::json_value(uint32_t val)
    {
        m_type = json_type::json_type_int;
        m_data._uint = val;
    }

    json_value::json_value(int64_t val)
    {
        m_type = json_type::json_type_int;
        m_data._int = val;
    }

    json_value::json_value(uint64_t val)
    {
        m_type = json_type::json_type_uint;
        m_data._uint = val;
    }

    json_value::json_value(json_float val)
    {
        m_type = json_type::json_type_float;
        m_data._float = val;
    }

    json_value::json_value(const _tchar* val)
    {
        if (val)
        {
            m_type = json_type::json_type_string;
            m_data._string_ptr = new (std::nothrow) json_string(val);
        }
        else
        {
            m_type = json_type::json_type_string;
            m_data = { 0 };
        }
    }

    json_value::json_value(const json_string& val)
    {
        m_type = json_type::json_type_string;
        m_data._string_ptr = new (std::nothrow) json_string(val);
    }

    json_value::json_value(const json_object& val)
    {
        m_type = json_type::json_type_object;
        m_data._object_ptr = new (std::nothrow) json_object(val);
    }

    json_value::json_value(const json_array& val)
    {
        m_type = json_type::json_type_array;
        m_data._array_ptr = new (std::nothrow) json_array(val);
    }

    json_value::json_value(const json_value& r)
    {
        m_type = r.m_type;

        switch (m_type)
        {
        case json_type::json_type_string:
        {
            m_data._string_ptr = new (std::nothrow) json_string(*r.m_data._string_ptr);
        }
        break;
        case json_type::json_type_object:
        {
            m_data._object_ptr = new (std::nothrow) json_object(*r.m_data._object_ptr);
        }
        break;
        case json_type::json_type_array:
        {
            m_data._array_ptr = new (std::nothrow) json_array(*r.m_data._array_ptr);
        }
        break;
        default:
        {
            m_data = r.m_data;
        }
        break;
        }
    }

    json_value::json_value(json_string&& val)
    {
        m_type = json_type::json_type_string;
        m_data._string_ptr = new (std::nothrow) json_string(std::move(val));
    }

    json_value::json_value(json_object&& val)
    {
        m_type = json_type::json_type_object;
        m_data._object_ptr = new (std::nothrow) json_object(std::move(val));
    }

    json_value::json_value(json_array&& val)
    {
        m_type = json_type::json_type_array;
        m_data._array_ptr = new (std::nothrow) json_array(std::move(val));
    }

    json_value::json_value(json_value&& r) noexcept
    {
        m_type = r.m_type;
        m_data = r.m_data;

        r.m_data = { 0 };
        r.m_type = json_type::json_type_null;
    }

    inline void json_value::_reset_type(json_type type)
    {
        if (this == &_get_none())
        {
            return;
        }

        if (type != m_type)
        {
            clear();
            m_type = type;
        }
    }

    json_value& json_value::operator = (nullptr_t)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        _reset_type(json_type::json_type_null);
        return *this;
    }

    json_value& json_value::operator = (json_type type)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        _reset_type(type);
        return *this;
    }

    json_value& json_value::operator = (json_bool val)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        _reset_type(json_type::json_type_bool);
        m_data._bool = val;
        return *this;
    }

    json_value& json_value::operator = (int32_t val)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        _reset_type(json_type::json_type_int);
        m_data._int = val;
        return *this;
    }

    json_value& json_value::operator = (uint32_t val)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        _reset_type(json_type::json_type_int);
        m_data._uint = val;
        return *this;
    }

    json_value& json_value::operator = (int64_t val)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        _reset_type(json_type::json_type_int);
        m_data._int = val;
        return *this;
    }

    json_value& json_value::operator = (uint64_t val)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        _reset_type(json_type::json_type_uint);
        m_data._uint = val;
        return *this;
    }

    json_value& json_value::operator = (json_float val)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        _reset_type(json_type::json_type_float);
        m_data._float = val;
        return *this;
    }

    json_value& json_value::operator = (const _tchar* val)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        _reset_type(json_type::json_type_string);
        if (nullptr == m_data._string_ptr)
        {
            m_data._string_ptr = new (std::nothrow) json_string(val);
        }
        return *this;
    }

    json_value& json_value::operator = (const json_string& val)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        _reset_type(json_type::json_type_string);
        if (nullptr == m_data._string_ptr)
        {
            m_data._string_ptr = new (std::nothrow) json_string(val);
        }
        return *this;
    }

    json_value& json_value::operator = (const json_object& val)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        _reset_type(json_type::json_type_object);
        if (nullptr == m_data._object_ptr)
        {
            m_data._object_ptr = new (std::nothrow) json_object(val);
        }
        return *this;
    }

    json_value& json_value::operator = (const json_array& val)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        _reset_type(json_type::json_type_array);
        if (nullptr == m_data._array_ptr)
        {
            m_data._array_ptr = new (std::nothrow) json_array(val);
        }
        return *this;
    }

    json_value& json_value::operator = (const json_value& r)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        if (&r != this)
        {
            clear();
            m_type = r.m_type;

            switch (m_type)
            {
            case json_type::json_type_string:
            {
                m_data._string_ptr = new (std::nothrow) json_string(*r.m_data._string_ptr);
            }
            break;
            case json_type::json_type_object:
            {
                m_data._object_ptr = new (std::nothrow) json_object(*r.m_data._object_ptr);
            }
            break;
            case json_type::json_type_array:
            {
                m_data._array_ptr = new (std::nothrow) json_array(*r.m_data._array_ptr);
            }
            break;
            default:
            {
                m_data = r.m_data;
            }
            break;
            }
        }

        return *this;
    }

    json_value& json_value::operator = (json_string&& val)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        _reset_type(json_type::json_type_string);
        if (nullptr == m_data._string_ptr)
        {
            m_data._string_ptr = new (std::nothrow) json_string(std::move(val));
        }
        return *this;
    }

    json_value& json_value::operator = (json_object&& val)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        _reset_type(json_type::json_type_object);
        if (nullptr == m_data._object_ptr)
        {
            m_data._object_ptr = new (std::nothrow) json_object(std::move(val));
        }
        return *this;
    }

    json_value& json_value::operator = (json_array&& val)
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        _reset_type(json_type::json_type_array);
        if (nullptr == m_data._array_ptr)
        {
            m_data._array_ptr = new (std::nothrow) json_array(std::move(val));
        }
        return *this;
    }

    json_value& json_value::operator = (json_value&& r) noexcept
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        if (&r != this)
        {
            clear();
            m_type = r.m_type;
            m_data = r.m_data;

            r.m_data = { 0 };
            r.m_type = json_type::json_type_null;
        }

        return *this;
    }

    json_value& json_value::operator[](const _tstring& val_name) noexcept
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        if (!is_object())
        {
            return _get_none();
        }

        if (nullptr == m_data._object_ptr)
        {
            m_data._object_ptr = new (std::nothrow) json_object;
            if (nullptr == m_data._object_ptr)
            {
                return _get_none();
            }
        }

        auto it_find = m_data._object_ptr->find(val_name);
        if (m_data._object_ptr->end() != it_find)
        {
            return it_find->second;
        }

        auto it_insert = m_data._object_ptr->insert(std::make_pair(val_name, json_value()));
        return it_insert.first->second;
    }

    json_value& json_value::operator[](size_t index) noexcept
    {
        if (this == &_get_none())
        {
            return _get_none();
        }

        if (json_type::json_type_array != m_type)
        {
            return _get_none();
        }

        if (nullptr == m_data._array_ptr)
        {
            m_data._array_ptr = new (std::nothrow) json_array;
            if (nullptr == m_data._array_ptr)
            {
                return _get_none();
            }
        }

        if (m_data._array_ptr->size() <= index)
        {
            m_data._array_ptr->resize(index + 1);
        }

        return (*m_data._array_ptr)[index];
    }

    json_value::~json_value()
    {
        clear();
    }

    json_type json_value::type() const
    {
        return m_type;
    }

    _tstring json_value::type_name() const
    {
        if (json_type::json_type_null == m_type) return _T("Null");
        if (json_type::json_type_bool == m_type) return _T("Bool");
        if (json_type::json_type_int == m_type) return _T("Integer");
        if (json_type::json_type_uint == m_type) return _T("Unsigned Integer");
        if (json_type::json_type_float == m_type) return _T("Float");
        if (json_type::json_type_string == m_type) return _T("String");
        if (json_type::json_type_object == m_type) return _T("Object");
        if (json_type::json_type_array == m_type) return _T("Array");
        return _T("None");
    }

    bool json_value::remove(const _tstring& name)
    {
        if (!is_object())
        {
            return false;
        }

        if (nullptr == m_data._object_ptr)
        {
            return false;
        }

        json_object& object = *m_data._object_ptr;
        auto it_find = object.find(name);
        if (object.end() == it_find)
        {
            return false;
        }

        object.erase(it_find);

        return true;
    }

    bool json_value::remove(const size_t index)
    {
        if (!is_array())
        {
            return false;
        }

        if (nullptr == m_data._array_ptr)
        {
            return false;
        }

        json_array& array = *m_data._array_ptr;
        if (index >= array.size())
        {
            return false;
        }

        array.erase(array.begin() + index);

        return true;

    }

    bool json_value::is_null() const
    {
        return json_type::json_type_null == m_type;
    }

    bool json_value::is_bool() const
    {
        return json_type::json_type_bool == m_type;
    }

    bool json_value::is_int() const
    {
        return json_type::json_type_int == m_type || json_type::json_type_uint == m_type;
    }

    bool json_value::is_float() const
    {
        return json_type::json_type_float == m_type;
    }

    bool json_value::is_number() const
    {
        return json_type::json_type_int == m_type || json_type::json_type_uint == m_type || json_type::json_type_float == m_type;
    }

    bool json_value::is_string() const
    {
        return json_type::json_type_string == m_type;
    }

    bool json_value::is_object() const
    {
        return json_type::json_type_object == m_type;
    }

    bool json_value::is_array() const
    {
        return json_type::json_type_array == m_type;
    }

    json_bool json_value::as_bool() const
    {
        if (!json_type::json_type_bool == m_type)
        {
            throw json_exception(_T("AsBool(): JsonType::eBool == m_Type"));
        }

        return m_data._bool;
    }

    json_int json_value::as_int() const
    {
        if (!(json_type::json_type_int == m_type || json_type::json_type_uint == m_type))
        {
            throw json_exception(_T("AsInt(): JsonType::eInteger == m_Type"));
        }

        return m_data._int;
    }

    json_uint json_value::as_uint() const
    {
        if (!(json_type::json_type_int == m_type || json_type::json_type_uint == m_type))
        {
            throw json_exception(_T("AsInt(): JsonType::eInteger == m_Type"));
        }

        return m_data._uint;
    }

    json_float json_value::as_float() const
    {
        if (!json_type::json_type_float == m_type)
        {
            throw json_exception(_T("AsFloat(): JsonType::eFloat == m_Type"));
        }

        return m_data._float;
    }

    json_string& json_value::as_string() const
    {
        if (!json_type::json_type_string == m_type)
        {
            throw json_exception(_T("AsString(): JsonType::eString == m_Type"));
        }

        return *m_data._string_ptr;
    }

    json_object& json_value::as_object() const
    {
        if (!json_type::json_type_object == m_type)
        {
            throw json_exception(_T("AsObject(): JsonType::eObject == m_Type"));
        }

        return *m_data._object_ptr;
    }

    json_array& json_value::as_array() const
    {
        if (!json_type::json_type_array == m_type)
        {
            throw json_exception(_T("AsArray(): JsonType::eArray == m_Type"));
        }

        return *m_data._array_ptr;
    }

    void json_value::clear()
    {
        if (json_type::json_type_null != m_type)
        {
            switch (m_type)
            {
            case json_type::json_type_string:
            {
                delete m_data._string_ptr;
            }
            break;
            case json_type::json_type_object:
            {
                delete m_data._object_ptr;
            }
            break;
            case json_type::json_type_array:
            {
                delete m_data._array_ptr;
            }
            break;
            }

            m_data = { 0 };
            m_type = json_type::json_type_null;
        }
    }

    size_t json_value::count(const _tstring& name) const
    {
        if (this == &_get_none())
        {
            return 0;
        }

        if (name.empty())
        {
            if (json_type::json_type_object == m_type && m_data._object_ptr)
            {
                return m_data._object_ptr->size();
            }

            if (json_type::json_type_array == m_type && m_data._array_ptr)
            {
                return m_data._array_ptr->size();
            }

            return 1;
        }

        auto it_find = m_data._object_ptr->find(name);
        if (m_data._object_ptr->end() == it_find)
        {
            return 0;
        }

        return it_find->second.count();
    }

    bool json_value::parse(const _tstring& text)
    {
        clear();
        const _tchar* end_ptr = nullptr;
        return _parse(text.c_str(), *this, &end_ptr);
    }

    bool json_value::parse_from_file(const _tstring& strPath)
    {

        std::string str_utf8;
        std::wstring str_utf16;
        _tstring read_text;

        clear();
        do
        {
            std::ifstream input_file(strPath, std::ios::binary | std::ios::in);
            if (!input_file.is_open())
            {
                return false;
            }

            input_file.seekg(0, std::ios::end);
            std::streamoff text_size = input_file.tellg();
            input_file.seekg(0, std::ios::beg);

            std::string text_buffer(text_size, 0);
            input_file.read((char*)&text_buffer[0], text_size);
            size_t byte_count = (size_t)input_file.gcount();
            input_file.close();

            if (0 == byte_count)
            {
                break;
            }

            int32_t utf8_length = _utf8_to_utf16(text_buffer.data(), text_buffer.size(), &str_utf8, &str_utf16);

#ifdef _UNICODE
            if (utf8_length > 0)
            {
                read_text = str_utf16;
                break;
            }
#else
            if (utf8_length > 0)
            {
                read_text = str_utf8;
                break;
            }
#endif

            int32_t utf16_length = _utf16_to_utf8(text_buffer.data(), text_buffer.size(), &str_utf8, &str_utf16);

#ifdef _UNICODE
            if (utf16_length > 0)
            {
                read_text = str_utf16;
                break;
            }
#else
            if (utf16_length > 0)
            {
                read_text = str_utf8;
                break;
            }
#endif

        } while (false);

        const _tchar* end_ptr = nullptr;
        return _parse(read_text.c_str(), *this, &end_ptr);
    }

    _tstring json_value::dump(int indent/* = 0*/, bool flag_escape/* = false*/) const
    {
        _tstring result_text;
        std::vector<_tstring> indent_text({_T("")});
        _dump(result_text, indent_text, 0, indent, flag_escape);
        return result_text;
    }

    bool json_value::dump_to_file(const _tstring& strPath, int indent/* = 0*/, bool flag_escape/* = false*/, json_encoding encoding/* = json_encoding::json_encoding_auto*/)
    {
        _tstring dump_text;
        std::vector<_tstring> indent_text({_T("")});
        _dump(dump_text, indent_text, 0, indent, flag_escape);

        std::string str_utf8;
        std::wstring str_utf16;
        _tstring result_text;

        str_utf16.push_back((uint16_t)0xFEFF);

        std::ofstream output_file(strPath, std::ios::binary | std::ios::out);
        if (!output_file.is_open())
        {
            return false;
        }

#ifdef _UNICODE
        result_text = str_utf16;

        if (json_encoding::json_encoding_utf16 == encoding || json_encoding::json_encoding_auto == encoding)
        {
            result_text += dump_text;
            output_file.write((const char*)result_text.data(), result_text.size() * sizeof(_tchar));
            output_file.close();
        }

        if (json_encoding::json_encoding_utf8 == encoding)
        {
            int32_t utf8_length = _utf16_to_utf8(dump_text.c_str(), (size_t)-1, &str_utf8, nullptr);
            if (utf8_length >= 0)
            {
                output_file.write((const char*)str_utf8.data(), str_utf8.size() * sizeof(char));
                output_file.close();
            }
        }

#else
        result_text = str_utf8;

        if (json_encoding::json_encoding_utf8 == encoding || json_encoding::json_encoding_auto == encoding)
        {
            result_text += dump_text;
            output_file.write((const char*)result_text.data(), result_text.size() * sizeof(_tchar));
            output_file.close();
        }

        if (json_encoding::json_encoding_utf16 == encoding)
        {
            int32_t utf8_length = _utf8_to_utf16(dump_text.c_str(), (size_t)-1, nullptr, &str_utf16);
            if (utf8_length >= 0)
            {
                output_file.write((const char*)str_utf16.data(), str_utf16.size() * sizeof(wchar_t));
                output_file.close();
            }
        }

#endif

        return true;
    }

    bool json_value::_parse_number(const _tchar* data_ptr, json_value& val, const _tchar** end_ptr)
    {
        // [-]?[0-9]+\.[0-9]+[eE]?[-+]?[0-9]+
        const _tchar* start_ptr = data_ptr;
        bool flag_negative = false;
        bool flag_dot = false;
        bool flag_exponent = false;
        bool result_flag = false;

        do
        {
            if (_T('-') == *data_ptr)
            {
                flag_negative = true;
                data_ptr++;
            }

            if (!_skip_digit(data_ptr, &data_ptr))
            {
                result_flag = false;
                break;
            }

            if (_T('.') == *data_ptr)
            {
                flag_dot = true;
                data_ptr++;
            }

            if (flag_dot)
            {
                if (!_skip_digit(data_ptr, &data_ptr))
                {
                    break;
                }
            }

            if (_T('E') == *data_ptr || _T('e') == *data_ptr)
            {
                flag_exponent = true;
                data_ptr++;

                if (_T('-') == *data_ptr || _T('+') == *data_ptr)
                {
                    data_ptr++;
                }

                if (!_skip_digit(data_ptr, &data_ptr))
                {
                    break;
                }
            }

            _tstring number_text(start_ptr, data_ptr - start_ptr);

            if (flag_dot || flag_exponent)
            {
                val = _json_tcstod(number_text.c_str(), nullptr);
            }
            else
            {
                if (flag_negative)
                {
                    val = (int64_t)_json_tcstoll(number_text.c_str(), nullptr, 10);
                }
                else
                {
                    val = (uint64_t)_json_tcstoull(number_text.c_str(), nullptr, 10);
                }

                if (ERANGE == errno)
                {
                    val = _json_tcstod(number_text.c_str(), nullptr);
                    errno = 0;
                }
            }

            result_flag = true;

        } while (false);

        if (end_ptr)
        {
            *end_ptr = data_ptr;
        }

        return result_flag;
    }

    bool json_value::_parse_unicode(const _tchar* data_ptr, _tstring& val, const _tchar** end_ptr)
    {
        uint32_t cp32 = 0;
        bool result_flag = false;

        do
        {
            if (!_get_utf16_code_point(data_ptr, &cp32, &data_ptr))
            {
                break;
            }

            // High bits
            if (cp32 >= 0xD800 && cp32 <= 0xDBFF)
            {
                cp32 -= 0xD800;

                if (0 != _json_tcsncmp(_T(R"(\u)"), data_ptr, 2))
                {
                    break;
                }

                data_ptr += 2;

                uint32_t cp_low = 0;
                if (!_get_utf16_code_point(data_ptr, &cp_low, &data_ptr))
                {
                    break;
                }

                // Low Bits
                if (cp_low >= 0xDC00 && cp_low <= 0xDFFF)
                {
                    cp_low -= 0xDC00;

                    cp32 = 0x10000 + ((cp32 << 10) | cp_low);
#ifdef _UNICODE
                    uint16_t cp = (uint16_t)(cp32 - 0x10000);
                    uint16_t cp32_high = (uint16_t)(cp >> 10) + 0xD800;
                    uint16_t cp32_low = (uint16_t)(cp & 0x3FF) + 0xDC00;
                    val.push_back(cp32_high);
                    val.push_back(cp32_low);
#else
                    val += _get_utf8_text_for_code_point(cp32);
#endif
                }
                else
                {
                    break;
                }
            }
            else
            {
#ifdef _UNICODE
                val.push_back((_tchar)cp32);
#else
                val += _get_utf8_text_for_code_point(cp32);
#endif
            }

            result_flag = true;

        } while (false);

        if (end_ptr)
        {
            *end_ptr = data_ptr;
        }

        return result_flag;
    }

    bool json_value::_parse_string(const _tchar* data_ptr, _tstring& val, const _tchar** end_ptr)
    {
        bool flag_abort = false;

        data_ptr = _skip_whitespace(data_ptr);
        if (_T('\"') != *data_ptr)
        {
            return false;
        }

        data_ptr++;

        while (_T('\0') != *data_ptr)
        {
            _tchar ch = *data_ptr;
            if (_T('\"') == ch)
            {
                break;
            }

            if (_T('\\') == ch)
            {
                data_ptr++;
                ch = *data_ptr;

                switch (ch)
                {
                case _T('\"'):
                {
                    val.push_back(_T('\"'));
                }
                break;
                case _T('\\'):
                {
                    val.push_back(_T('\\'));
                }
                break;
                case _T('/'):
                {
                    val.push_back(_T('/'));
                }
                break;
                case _T('b'):
                {
                    val.push_back(_T('\b'));
                }
                break;
                case _T('n'):
                {
                    val.push_back(_T('\n'));
                }
                break;
                case _T('r'):
                {
                    val.push_back(_T('\r'));
                }
                break;
                case _T('t'):
                {
                    val.push_back(_T('\t'));
                }
                break;
                case _T('u'):
                {
                    data_ptr++;
                    if (!_parse_unicode(data_ptr, val, &data_ptr))
                    {
                        flag_abort = true;
                        break;
                    }
                    continue;
                }
                break;
                default:
                    data_ptr--;
                    flag_abort = true;
                    break;
                }
            }
            else
            {
                val.push_back(ch);
            }

            if (flag_abort)
            {
                break;
            }

            data_ptr++;
        }

        if (_T('\"') != *data_ptr || flag_abort)
        {
            *end_ptr = data_ptr;
            return false;
        }

        data_ptr++;

        if (end_ptr)
        {
            *end_ptr = data_ptr;
        }

        return true;
    }

    void json_value::_dump_int(_tstring& append_str, int64_t val) const
    {
        _tchar out_buffer[64] = { 0 };
        size_t length = _json_stprintf_s(out_buffer, 64, _T(FC_JSON_INT64_FORMAT), val);
        append_str.append(out_buffer, length);
    }

    void json_value::_dump_uint(_tstring& append_str, uint64_t val) const
    {
        _tchar out_buffer[64] = { 0 };
        size_t length = _json_stprintf_s(out_buffer, 64, _T(FC_JSON_UINT64_FORMAT), val);
        append_str.append(out_buffer, length);
    }

    void json_value::_dump_float(_tstring& append_str, double val) const
    {
        _tchar out_buffer[64] = { 0 };
        size_t length = _json_stprintf_s(out_buffer, 64, _T(FC_JSON_FLOAT_FORMAT), val);
        append_str.append(out_buffer, length);

        _tchar* ch_ptr = out_buffer;
        bool flag_dot = false;
        bool flag_exponent = false;

        while (_T('\0') != *ch_ptr)
        {
            _tchar ch = *ch_ptr;
            if (_T('.') == ch)
            {
                flag_dot = true;
            }
            else if (_T('e') == ch)
            {
                flag_exponent = true;
            }
            ch_ptr++;
        }

        if (!flag_dot && 0 == !flag_exponent)
        {
            append_str += _T(".0");
        }
    }

    void json_value::_dump_string(_tstring& append_str, const _tstring& text, bool flag_escape) const
    {
        const _tchar* data_ptr = text.c_str();

        while (_T('\0') != *data_ptr)
        {
            _utchar ch = *data_ptr;

            switch (ch)
            {
            case _T('\b'):
            {
                append_str += _T(R"(\b)");
            }
            break;
            case _T('\t'):
            {
                append_str += _T(R"(\t)");
            }
            break;
            case _T('\n'):
            {
                append_str += _T(R"(\n)");
            }
            break;
            case _T('\f'):
            {
                append_str += _T(R"(\f)");
            }
            break;
            case _T('\r'):
            {
                append_str += _T(R"(\r)");
            }
            break;
            case _T('\"'):
            {
                append_str += _T(R"(\")");
            }
            break;
            case _T('/'):
            {
                append_str += _T(R"(/)");
            }
            case _T('\\'):
            {
                append_str += _T(R"(\\")");
            }
            break;
            default:
            {
                if (ch < 0x80 || !flag_escape)
                {
                    append_str.push_back(ch);
                }
                else
                {
                    _get_unicode_string(append_str, data_ptr, &data_ptr);
                    continue;
                }
            }
            break;
            }

            data_ptr++;
        }
    }

    void json_value::_dump_object(_tstring& append_str, std::vector<_tstring>& indent_text, int depth, int indent, bool flag_escape) const
    {
        const json_object& object = *m_data._object_ptr;
        size_t size = object.size();

        append_str += _T("{");
        if (indent > 0)
        {
            depth++;

            if (indent_text.size() <= depth)
            {
                indent_text.emplace_back(_tstring(depth * indent, _T(' ')));
            }

            append_str += _T(FC_JSON_RETURN);

            for (const auto& item : object)
            {
                append_str += indent_text[depth];
                append_str += _T("\"");
                _dump_string(append_str, item.first, flag_escape);
                append_str += _T("\": ");
                item.second._dump(append_str, indent_text, depth, indent, flag_escape);
                size--;

                if (0 != size)
                {
                    append_str += _T(",");
                }

                append_str += _T(FC_JSON_RETURN);
            }

            depth--;
            append_str += indent_text[depth];
        }
        else
        {
            for (const auto& item : object)
            {
                append_str += _T("\"");
                _dump_string(append_str, item.first, flag_escape);
                append_str += _T("\":");
                item.second._dump(append_str, indent_text, depth, indent, flag_escape);
                size--;

                if (0 != size)
                {
                    append_str += _T(",");
                }
            }
        }
        append_str += _T("}");
    }

    void json_value::_dump_array(_tstring& append_str, std::vector<_tstring>& indent_text, int depth, int indent, bool flag_escape) const
    {
        const json_array& array = *m_data._array_ptr;
        size_t size = array.size();

        append_str += _T("[");
        if (indent > 0)
        {
            depth++;

            if (indent_text.size() <= depth)
            {
                indent_text.emplace_back(_tstring(depth * indent, _T(' ')));
            }

            append_str += _T(FC_JSON_RETURN);

            for (const auto& item : array)
            {
                append_str += indent_text[depth];
                item._dump(append_str, indent_text, depth, indent, flag_escape);
                size--;
                if (0 != size)
                {
                    append_str += _T(",");
                }

                append_str += _T(FC_JSON_RETURN);
            }

            depth--;
            append_str += indent_text[depth];
        }
        else
        {
            for (const auto& item : array)
            {
                item._dump(append_str, indent_text, depth, indent, flag_escape);
                size--;
                if (0 != size)
                {
                    append_str += _T(",");
                }
            }
        }
        append_str += _T("]");
    }

    void json_value::_dump(_tstring& append_str, std::vector<_tstring>& indent_text, int depth, int indent, bool flag_escape) const
    {
        if (indent < 0)
        {
            indent = 0;
        }

        switch (m_type)
        {
        case json_type::json_type_null:
        {
            append_str += _T("null");
        }
        break;
        case json_type::json_type_bool:
        {
            append_str += m_data._bool ? _T("true") : _T("false");
        }
        break;
        case json_type::json_type_int:
        {
            _dump_int(append_str, m_data._int);
        }
        break;
        case json_type::json_type_uint:
        {
            _dump_uint(append_str, m_data._uint);
        }
        break;
        case json_type::json_type_float:
        {
            _dump_float(append_str, m_data._float);
        }
        break;
        case json_type::json_type_string:
        {
            append_str += _T("\"");
            _dump_string(append_str, *m_data._string_ptr, flag_escape);
            append_str += _T("\"");
        }
        break;
        case json_type::json_type_object:
        {
            if (nullptr == m_data._array_ptr)
            {
                append_str += _T("{}");
                break;
            }

            if (m_data._array_ptr->empty())
            {
                append_str += _T("{}");
                break;
            }

            _dump_object(append_str, indent_text, depth, indent, flag_escape);
        }
        break;
        case json_type::json_type_array:
        {
            if (nullptr == m_data._array_ptr)
            {
                append_str += _T("[]");
                break;
            }

            if (m_data._array_ptr->empty())
            {
                append_str += _T("[]");
                break;
            }

            _dump_array(append_str, indent_text, depth, indent, flag_escape);
        }
        break;
        default:
        {
        }
        break;
        }
    }

    json_value& json_value::_get_none()
    {
        static json_value val(json_type::json_type_null);
        return val;
    }

    bool _skip_digit(const _tchar* data_ptr, const _tchar** end_ptr)
    {
        if (0 == _json_istdigit(*data_ptr))
        {
            return false;
        }

        while (_json_istdigit(*data_ptr))
        {
            data_ptr++;
        }

        *end_ptr = data_ptr;

        return true;
    }

    bool json_value::_parse_object(const _tchar* data_ptr, json_value& val, const _tchar** end_ptr)
    {
        bool result_flag = false;

        if (_T('{') == *data_ptr)
        {
            data_ptr++;
        }

        val._reset_type(json_type::json_type_object);
        _tstring value_name;
        while (_T('\0') != *data_ptr)
        {
            data_ptr = _skip_whitespace(data_ptr);
            if (_T('}') == *data_ptr)
            {
                result_flag = true;
                data_ptr++;
                break;
            }

            value_name.clear();
            if (!_parse_string(data_ptr, value_name, &data_ptr))
            {
                break;
            }

            data_ptr = _skip_whitespace(data_ptr);
            if (_T(':') != *data_ptr)
            {
                break;
            }
            data_ptr++;

            json_value value_data;
            if (!_parse_value(data_ptr, value_data, &data_ptr))
            {
                break;
            }

            if (nullptr == val.m_data._object_ptr)
            {
                val.m_data._object_ptr = new (std::nothrow) json_object;
            }

            if (val.m_data._object_ptr)
            {
                val.m_data._object_ptr->emplace(value_name, std::move(value_data));
            }

            data_ptr = _skip_whitespace(data_ptr);
            if (_T(',') == *data_ptr)
            {
                data_ptr++;
            }
            else if (_T('}') == *data_ptr)
            {
                result_flag = true;
                data_ptr++;
                break;
            }
            else
            {
                break;
            }
        }

        if (end_ptr)
        {
            *end_ptr = data_ptr;
        }

        return result_flag;
    }

    bool json_value::_parse_array(const _tchar* data_ptr, json_value& val, const _tchar** end_ptr)
    {
        bool result_flag = false;

        if (_T('[') == *data_ptr)
        {
            data_ptr++;
        }

        val._reset_type(json_type::json_type_array);
        while (_T('\0') != *data_ptr)
        {
            data_ptr = _skip_whitespace(data_ptr);
            if (_T(']') == *data_ptr)
            {
                result_flag = true;
                data_ptr++;
                break;
            }

            json_value value_data;
            if (!_parse_value(data_ptr, value_data, &data_ptr))
            {
                break;
            }

            if (nullptr == val.m_data._array_ptr)
            {
                val.m_data._array_ptr = new (std::nothrow) json_array;
            }

            if (val.m_data._array_ptr)
            {
                val.m_data._array_ptr->emplace_back(std::move(value_data));
            }

            data_ptr = _skip_whitespace(data_ptr);
            if (_T(',') == *data_ptr)
            {
                data_ptr++;
            }
            else if (_T(']') == *data_ptr)
            {
                result_flag = true;
                data_ptr++;
                break;
            }
            else
            {
                break;
            }
        }

        if (end_ptr)
        {
            *end_ptr = data_ptr;
        }

        return result_flag;
    }

    bool json_value::_parse_value(const _tchar* data_ptr, json_value& val, const _tchar** end_ptr)
    {
        data_ptr = _skip_whitespace(data_ptr);
        bool result_flag = false;
        bool abort_flag = false;

        do
        {
            _tchar ch = *data_ptr;

            switch (ch)
            {
            case _T('{'):
            {
                if (!_parse_object(data_ptr, val, &data_ptr))
                {
                    abort_flag = true;
                    break;
                }
            }
            break;
            case _T('['):
            {
                if (!_parse_array(data_ptr, val, &data_ptr))
                {
                    abort_flag = true;
                    break;
                }
            }
            break;
            case _T('\"'):
            {
                _tstring str_value;
                if (!_parse_string(data_ptr, str_value, &data_ptr))
                {
                    abort_flag = true;
                    break;
                }
                val = std::move(str_value);
            }
            break;
            case _T('-'):
            {
                if (!_parse_number(data_ptr, val, &data_ptr))
                {
                    abort_flag = true;
                    break;
                }
            }
            break;
            default:
            {
                if (_json_istdigit(ch))
                {
                    if (!_parse_number(data_ptr, val, &data_ptr))
                    {
                        abort_flag = true;
                        break;
                    }
                }
                else if (0 == _json_tcsncmp(_T("null"), data_ptr, 4))
                {
                    val = json_value(json_type::json_type_null);
                    data_ptr += 4;
                }
                else if (0 == _json_tcsncmp(_T("true"), data_ptr, 4))
                {
                    val = true;
                    data_ptr += 4;
                }
                else if (0 == _json_tcsncmp(_T("false"), data_ptr, 5))
                {
                    val = false;
                    data_ptr += 5;
                }
                else
                {
                    abort_flag = true;
                    break;
                }
            }
            break;
            }

            if (!abort_flag)
            {
                result_flag = true;
            }

        } while (false);

        if (*end_ptr)
        {
            *end_ptr = data_ptr;
        }

        return result_flag;
    }

    bool json_value::_parse(const _tchar* data_ptr, json_value& val, const _tchar** end_ptr)
    {
        bool result_flag = false;
        data_ptr = _skip_bom(data_ptr);
        if (_parse_value(data_ptr, val, &data_ptr))
        {
            if (_T('\0') != *data_ptr)
            {
                val = json_type::json_type_null;
            }
            else
            {
                result_flag = true;
            }
        }

        if (!result_flag)
        {
            val.clear();
        }

        if (end_ptr)
        {
            *end_ptr = data_ptr;
        }

        return result_flag;
    }

    bool _get_utf16_code_point(const _tchar* data_ptr, uint32_t* code_point_ptr, const _tchar** end_ptr)
    {
        _tchar text_buffer[16] = { 0 };
        _tchar* ch_end_ptr = nullptr;
        bool result_flag = false;

        do
        {
            int count = 0;
            for (count = 0; count < 4; count++)
            {
                _tchar ch = *data_ptr;
                if (0 == _json_istxdigit(ch))
                {
                    break;
                }

                text_buffer[count] = ch;
                data_ptr++;
            }

            if (4 != count)
            {
                break;
            }

            if (code_point_ptr)
            {
                *code_point_ptr = _json_tcstol(text_buffer, &ch_end_ptr, 16);
            }

            result_flag = true;

        } while (false);

        if (end_ptr)
        {
            *end_ptr = data_ptr;
        }

        return result_flag;
    }

    bool _get_unicode_string(_tstring& append_str, const _tchar* data_ptr, const _tchar** end_ptr)
    {
        _utchar ch = *data_ptr;

#ifdef _UNICODE
        _tchar text_buffer[32] = { 0 };
        _json_stprintf_s(text_buffer, sizeof(text_buffer) / sizeof(_tchar), _T(R"(\u%.4x)"), ch);
        append_str += text_buffer;
        data_ptr++;

#else
        if (ch >= 0xC0)
        {
            // The number of bytes used to obtain characters.
            size_t byte_count = 0;
            uint32_t cp32 = 0;

            if (ch >= 0xE0 && ch <= 0xEF)
            {
                byte_count = 3;
                cp32 = ch & 0x0F;
            }
            else if (ch >= 0xC0 && ch <= 0xDF)
            {
                byte_count = 2;
                cp32 = ch & 0x1F;
            }
            else if (ch >= 0xF0 && ch <= 0xF7)
            {
                byte_count = 4;
                cp32 = ch & 0x07;
            }
            else if (ch >= 0xF8 && ch <= 0xFB)
            {
                byte_count = 5;
                cp32 = ch & 0x03;
            }
            else if (ch >= 0xFC && ch <= 0xFD)
            {
                byte_count = 6;
                cp32 = ch & 0x01;
            }

            if (0 == byte_count)
            {
                return false;
            }

            for (size_t i = 1; i < byte_count; i++)
            {
                cp32 = cp32 << 6;
                cp32 |= data_ptr[i] & 0x3F;
            }

            char text_buffer[32] = { 0 };
            if (cp32 < 0x10000)
            {
                snprintf(text_buffer, sizeof(text_buffer), R"(\u%.4x)", cp32);
                append_str.append(text_buffer, 6);
            }
            else
            {
                uint32_t cp = (uint16_t)(cp32 - 0x10000);
                uint16_t cp32_high = (uint16_t)(cp >> 10) + 0xD800;
                uint16_t cp32_low = (uint16_t)(cp & 0x3FF) + 0xDC00;

                snprintf(text_buffer, sizeof(text_buffer), R"(\u%.4x\u%.4x)", cp32_high, cp32_low);
                append_str.append(text_buffer, 12);
            }

            data_ptr += byte_count;
        }
#endif

        if (end_ptr)
        {
            *end_ptr = data_ptr;
        }

        return false;
    }

    std::string _get_utf8_text_for_code_point(uint32_t cp32)
    {
        char text_buffer[16] = { 0 };

        // 1byte 0xxxxxxx
        if (cp32 >= 0x00000000 && cp32 <= 0x0000007F)
        {
            text_buffer[0] = (uint8_t)cp32;
            text_buffer[1] = 0;
        }

        // 2bytes 110xxxxx 10xxxxxx
        if (cp32 >= 0x00000080 && cp32 <= 0x000007FF)
        {
            text_buffer[0] = ((cp32 >>  6) & 0x1F) | 0xC0;
            text_buffer[1] = ((cp32 & 0x3F)) | 0x80;
            text_buffer[2] = 0;
        }

        // 3bytes 1110xxxx 10xxxxxx 10xxxxxx
        if (cp32 >= 0x00000800 && cp32 <= 0x0000FFFF)
        {
            text_buffer[0] = ((cp32 >> 12) & 0x0F) | 0xE0;
            text_buffer[1] = ((cp32 >>  6) & 0x3F) | 0x80;
            text_buffer[2] = ((cp32 & 0x3F)) | 0x80;
            text_buffer[3] = 0;
        }

        // 4bytes 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        if (cp32 >= 0x00010000 && cp32 <= 0x001FFFFF)
        {
            text_buffer[0] = ((cp32 >> 18) & 0x07) | 0xF0;
            text_buffer[1] = ((cp32 >> 12) & 0x3F) | 0x80;
            text_buffer[2] = ((cp32 >>  6) & 0x3F) | 0x80;
            text_buffer[3] = ((cp32 & 0x3F)) | 0x80;
            text_buffer[4] = 0;
        }

        // 5bytes 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        if (cp32 >= 0x00200000 && cp32 <= 0x03FFFFFF)
        {
            text_buffer[0] = ((cp32 >> 24) & 0x03) | 0xF8;
            text_buffer[1] = ((cp32 >> 18) & 0x3F) | 0x80;
            text_buffer[2] = ((cp32 >> 12) & 0x3F) | 0x80;
            text_buffer[3] = ((cp32 >>  6) & 0x3F) | 0x80;
            text_buffer[4] = ((cp32 & 0x3F)) | 0x80;
            text_buffer[5] = 0;
        }

        // 6bytes 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        if (cp32 >= 0x04000000 && cp32 <= 0x7FFFFFFF)
        {
            text_buffer[0] = ((cp32 >> 30) & 0x01) | 0xFC;
            text_buffer[1] = ((cp32 >> 24) & 0x3F) | 0x80;
            text_buffer[2] = ((cp32 >> 18) & 0x3F) | 0x80;
            text_buffer[3] = ((cp32 >> 12) & 0x3F) | 0x80;
            text_buffer[4] = ((cp32 >>  6) & 0x3F) | 0x80;
            text_buffer[5] = ((cp32 & 0x3F)) | 0x80;
            text_buffer[6] = 0;
        }

        return text_buffer;
    }

    int32_t _utf8_to_utf16(const void* data_ptr, size_t size/* = -1*/, std::string* text_utf8_ptr/* = nullptr*/, std::wstring* text_utf16_ptr/* = nullptr*/)
    {
        const uint8_t* ch_data_ptr = (const uint8_t*)data_ptr;
        std::wstring text_out_utf16;
        std::string text_out_utf8;
        uint32_t cp32 = 0;
        int32_t byte_count = 0;
        int32_t ch_count = 0;
        bool result_flag = true;
        bool flag_bom = true;

        if (text_utf8_ptr)
        {
            text_out_utf8 += *text_utf8_ptr;
        }

        if (text_utf16_ptr)
        {
            text_out_utf16 += *text_utf16_ptr;
        }

        while ((0 != *ch_data_ptr) && (0 != size))
        {
            uint8_t ch = *ch_data_ptr;

            if (ch < 0x7F)
            {
                cp32 = ch;
                ch_count++;
            }
            else
            {
                if (0 == byte_count)
                {
                    cp32 = 0;
                    if (ch >= 0xC0)
                    {
                        if (ch >= 0xC0 && ch <= 0xDF)
                        {
                            byte_count = 2;
                            cp32 = ch & 0x1F;
                        }
                        else if (ch >= 0xE0 && ch <= 0xEF)
                        {
                            byte_count = 3;
                            cp32 = ch & 0x0F;
                        }
                        else if (ch >= 0xF0 && ch <= 0xF7)
                        {
                            byte_count = 4;
                            cp32 = ch & 0x07;
                        }
                        else if (ch >= 0xF8 && ch <= 0xFB)
                        {
                            byte_count = 5;
                            cp32 = ch & 0x03;
                        }
                        else if (ch >= 0xFC && ch <= 0xFD)
                        {
                            byte_count = 6;
                            cp32 = ch & 0x01;
                        }

                        if (0 == byte_count)
                        {
                            result_flag = false;
                            break;
                        }

                        if (0xEF == ch && 3 == byte_count)
                        {
                            flag_bom = true;
                        }

                        byte_count--;
                    }
                    else
                    {
                        result_flag = false;
                        break;
                    }
                }
                else
                {
                    if (0x80 != (ch & 0xC0))
                    {
                        result_flag = false;
                        break;
                    }

                    if (flag_bom)
                    {
                        if (0xBB != ch && 2 == byte_count)
                        {
                            flag_bom = false;
                        }

                        if (0xBF != ch && 1 == byte_count)
                        {
                            flag_bom = false;
                        }
                    }

                    cp32 = cp32 << 6;
                    cp32 |= ch & 0x3F;

                    byte_count--;

                    if (0 == byte_count)
                    {
                        if (flag_bom)
                        {
                            flag_bom = false;
                            ch_data_ptr++;
                            continue;
                        }

                        ch_count++;
                    }
                }
            }

            if (0 == byte_count)
            {
                if (text_utf8_ptr)
                {
                    text_out_utf8 += _get_utf8_text_for_code_point(cp32);
                }

                if (text_utf16_ptr)
                {
                    if (cp32 < 0x10000)
                    {
                        text_out_utf16.push_back((uint16_t)(cp32 & 0xFFFF));
                    }
                    else
                    {
                        uint16_t cp = (uint16_t)(cp32 - 0x10000);
                        uint16_t cp32_high = (uint16_t)(cp >> 10) + 0xD800;
                        uint16_t cp32_low = (uint16_t)(cp & 0x3FF) + 0xDC00;

                        text_out_utf16.push_back(cp32_high);
                        text_out_utf16.push_back(cp32_low);
                    }
                }
            }

            ch_data_ptr++;

            if (-1 != size)
            {
                size--;
            }
        }

        if (!result_flag)
        {
            return -1;
        }

        if (text_utf8_ptr)
        {
            *text_utf8_ptr = std::move(text_out_utf8);
        }

        if (text_utf16_ptr)
        {
            *text_utf16_ptr = std::move(text_out_utf16);
        }

        return ch_count;
    }

    int32_t _utf16_to_utf8(const void* data_ptr, size_t size/* = -1*/, std::string* text_utf8_ptr/* = nullptr*/, std::wstring* text_utf16_ptr/* = nullptr*/)
    {
        const uint16_t* ch_data_ptr = (const uint16_t*)data_ptr;
        std::wstring text_out_utf16;
        std::string text_out_utf8;
        uint32_t cp32 = 0;
        uint16_t cp32_high = 0;
        uint16_t cp32_low = 0;
        uint16_t cp16 = 0;
        int32_t byte_count = 0;
        int32_t ch_count = 0;
        bool flag_big_endian = false;
        bool flag_little_endian = false;
        bool result_flag = true;

        if (text_utf8_ptr)
        {
            text_out_utf8 += *text_utf8_ptr;
        }

        if (text_utf16_ptr)
        {
            text_out_utf16 += *text_utf16_ptr;
        }

        if (-1 != size)
        {
            if ((size < 2) || (0 != (size % 2)))
            {
                return -1;
            }
        }

        while ((0 != *ch_data_ptr) && (0 != size))
        {
            cp16 = *ch_data_ptr;

            if (0xFFFE == cp16 || 0xFEFF == cp16)
            {
                if (0 == byte_count)
                {
                    if (0xFFFE == cp16)
                    {
                        flag_big_endian = true;
                    }

                    if (0xFEFF == cp16)
                    {
                        flag_little_endian = true;
                    }
                }
                else
                {
                    result_flag = false;
                    break;
                }

                if (flag_big_endian && flag_little_endian)
                {
                    result_flag = false;
                    break;
                }

                ch_data_ptr++;

                if (-1 != size)
                {
                    size -= 2;
                }

                continue;
            }

            if (flag_big_endian)
            {
                cp16 = ((cp16 >> 8) | (cp16 << 8));
            }

            if (!(cp16 >= 0xD800 && cp16 <= 0xDFFF))
            {
                if (cp32_high > 0)
                {
                    result_flag = false;
                    break;
                }

                cp32 = cp16;
                ch_count++;
            }
            else
            {
                if (0 == byte_count)
                {
                    if (cp16 >= 0xD800 && cp16 <= 0xDBFF)
                    {
                        cp32_high = (cp16 - 0xD800);
                        byte_count = 1;
                    }
                    else
                    {
                        result_flag = false;
                        break;
                    }
                }
                else
                {
                    if (1 == byte_count)
                    {
                        if ((cp16 >= 0xDC00) && (cp16 <= 0xDFFF))
                        {
                            cp32_low = (cp16 - 0xDC00);
                            cp32 = 0x10000 + ((uint32_t)cp32_high << 10 | cp32_low);
                            cp32_low = 0;
                            cp32_high = 0;
                        }
                        else
                        {
                            result_flag = false;
                            break;
                        }
                    }

                    byte_count--;

                    if (0 == byte_count)
                    {
                        ch_count++;
                    }
                }
            }

            if (0 == byte_count)
            {
                if (text_utf8_ptr)
                {
                    text_out_utf8 += _get_utf8_text_for_code_point(cp32);
                }

                if (text_utf16_ptr)
                {
                    if (cp32 < 0x10000)
                    {
                        text_out_utf16.push_back((uint16_t)(cp32 & 0xFFFF));
                    }
                    else
                    {
                        uint16_t cp = (uint16_t)(cp32 - 0x10000);
                        uint16_t cpHi = (uint16_t)(cp >> 10) + 0xD800;
                        uint16_t cp_low = (uint16_t)(cp & 0x3FF) + 0xDC00;

                        text_out_utf16.push_back(cpHi);
                        text_out_utf16.push_back(cp_low);
                    }
                }
            }

            ch_data_ptr++;

            if (-1 != size)
            {
                size -= 2;
            }
        }

        if (!result_flag)
        {
            return -1;
        }

        if (text_utf8_ptr)
        {
            *text_utf8_ptr = std::move(text_out_utf8);
        }

        if (text_utf16_ptr)
        {
            *text_utf16_ptr = std::move(text_out_utf16);
        }

        return ch_count;
    }
}