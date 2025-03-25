#include "fcjson.h"
#include <cwctype>
#include <fstream>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#include <Windows.h>
#endif

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
#define __JSON_FUNCTION__        __FUNCTIONW__
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
#define __JSON_FUNCTION__        __FUNCTION__

#endif

    static std::string _get_utf8_text_for_code_point(uint32_t cp32);
    static bool _get_utf16_code_point(const _tchar* data_ptr, uint32_t* code_point_ptr, const _tchar** end_ptr);
    static bool _get_unicode_string(_tstring& append_str, const _tchar* data_ptr, const _tchar** end_ptr);
    static int32_t _utf8_to_utf16(const void* data_ptr, size_t size = -1, std::string* text_utf8_ptr = nullptr, std::wstring* text_utf16_ptr = nullptr);
    static int32_t _utf16_to_utf8(const void* data_ptr, size_t size = -1, std::string* text_utf8_ptr = nullptr, std::wstring* text_utf16_ptr = nullptr);
    static std::string _utf16_to_utf8(const std::wstring utf16);
    static std::wstring _utf8_to_utf16(const std::string utf8);
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

    json_value::json_value(json_type type) :
        m_data{ 0 },
        m_type(type)
    {
        if (json_type::json_type_string == m_type)
        {
            m_data._string_ptr = new (std::nothrow) json_string;
        }

        if (json_type::json_type_array == m_type)
        {
            m_data._array_ptr = new (std::nothrow) json_array;
        }

        if (json_type::json_type_object == m_type)
        {
            m_data._object_ptr = new (std::nothrow) json_object;
        }
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

    json_value::json_value(const json_string& r)
    {
        m_type = json_type::json_type_string;
        m_data._string_ptr = new (std::nothrow) json_string(r);
    }

    json_value::json_value(const json_object& r)
    {
        m_type = json_type::json_type_object;
        m_data._object_ptr = new (std::nothrow) json_object(r);
    }

    json_value::json_value(const json_array& r)
    {
        m_type = json_type::json_type_array;
        m_data._array_ptr = new (std::nothrow) json_array(r);
    }

    json_value::json_value(const json_bin& r)
    {
        m_type = json_type::json_type_bin;
        m_data._raw_ptr = new (std::nothrow) json_bin(r);

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

    json_value::json_value(json_string&& r)
    {
        m_type = json_type::json_type_string;
        m_data._string_ptr = new (std::nothrow) json_string(std::move(r));
    }

    json_value::json_value(json_object&& r)
    {
        m_type = json_type::json_type_object;
        m_data._object_ptr = new (std::nothrow) json_object(std::move(r));
    }

    json_value::json_value(json_array&& r)
    {
        m_type = json_type::json_type_array;
        m_data._array_ptr = new (std::nothrow) json_array(std::move(r));
    }

    json_value::json_value(json_bin&& r)
    {
        m_type = json_type::json_type_bin;
        m_data._raw_ptr = new (std::nothrow) json_bin(std::move(r));
    }

    json_value::json_value(json_value&& r) noexcept
    {
        m_type = r.m_type;
        m_data = r.m_data;

        r.m_data = { 0 };
        r.m_type = json_type::json_type_null;
    }

    void json_value::clear()
    {
        switch (m_type)
        {
        case json_type::json_type_string:
        {
            if (m_data._string_ptr)
            {
                delete m_data._string_ptr;
            }
        }
        break;
        case json_type::json_type_object:
        {
            if (m_data._object_ptr)
            {
                delete m_data._object_ptr;
            }
        }
        break;
        case json_type::json_type_array:
        {
            if (m_data._array_ptr)
            {
                delete m_data._array_ptr;
            }
        }
        break;
        case json_type::json_type_bin:
        {
            if (m_data._raw_ptr)
            {
                delete m_data._raw_ptr;
            }
        }
        break;
        }

        m_data = { 0 };
    }

    inline void json_value::_reset_type(json_type type)
    {
        if (this == &_get_none_value())
        {
            return;
        }

        clear();
        m_type = type;

        if (json_type::json_type_string == m_type)
        {
            m_data._string_ptr = new (std::nothrow) json_string;
        }

        if (json_type::json_type_array == m_type)
        {
            m_data._array_ptr = new (std::nothrow) json_array;
        }

        if (json_type::json_type_object == m_type)
        {
            m_data._object_ptr = new (std::nothrow) json_object;
        }

        if (json_type::json_type_bin == m_type)
        {
            m_data._raw_ptr = new (std::nothrow) json_bin;
        }
    }

    json_value& json_value::operator = (nullptr_t)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        _reset_type(json_type::json_type_null);
        return *this;
    }

    json_value& json_value::operator = (json_type type)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        _reset_type(type);
        return *this;
    }

    json_value& json_value::operator = (json_bool val)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        _reset_type(json_type::json_type_bool);
        m_data._bool = val;
        return *this;
    }

    json_value& json_value::operator = (int32_t val)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        _reset_type(json_type::json_type_int);
        m_data._int = val;
        return *this;
    }

    json_value& json_value::operator = (uint32_t val)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        _reset_type(json_type::json_type_int);
        m_data._uint = val;
        return *this;
    }

    json_value& json_value::operator = (int64_t val)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        _reset_type(json_type::json_type_int);
        m_data._int = val;
        return *this;
    }

    json_value& json_value::operator = (uint64_t val)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        _reset_type(json_type::json_type_uint);
        m_data._uint = val;
        return *this;
    }

    json_value& json_value::operator = (json_float val)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        _reset_type(json_type::json_type_float);
        m_data._float = val;
        return *this;
    }

    json_value& json_value::operator = (const _tchar* val)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        _reset_type(json_type::json_type_string);
        if (nullptr == m_data._string_ptr)
        {
            m_data._string_ptr = new (std::nothrow) json_string(val);
        }
        return *this;
    }

    json_value& json_value::operator = (const json_string& r)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        _reset_type(json_type::json_type_string);
        if (nullptr != m_data._string_ptr)
        {
            *m_data._string_ptr = r;
        }
        return *this;
    }

    json_value& json_value::operator = (const json_object& r)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        _reset_type(json_type::json_type_object);
        if (nullptr != m_data._object_ptr)
        {
            *m_data._object_ptr = r;
        }
        return *this;
    }

    json_value& json_value::operator = (const json_array& r)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        _reset_type(json_type::json_type_array);
        if (nullptr != m_data._array_ptr)
        {
            *m_data._array_ptr = r;
        }
        return *this;
    }

    json_value& json_value::operator = (const json_value& r)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
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
            case json_type::json_type_bin:
            {
                m_data._raw_ptr = new (std::nothrow) json_bin(*r.m_data._raw_ptr);
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

    json_value& json_value::operator = (json_string&& r)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        _reset_type(json_type::json_type_string);
        if (nullptr != m_data._string_ptr)
        {
            *m_data._string_ptr = std::move(r);
        }
        return *this;
    }

    json_value& json_value::operator = (json_object&& r)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        _reset_type(json_type::json_type_object);
        if (nullptr != m_data._object_ptr)
        {
            *m_data._object_ptr = std::move(r);
        }
        return *this;
    }

    json_value& json_value::operator = (json_array&& r)
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        _reset_type(json_type::json_type_array);
        if (nullptr != m_data._array_ptr)
        {
            *m_data._array_ptr = std::move(r);
        }
        return *this;
    }

    json_value& json_value::operator = (json_value&& r) noexcept
    {
        if (this == &_get_none_value())
        {
            return _get_none_value();
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
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        if (!is_object())
        {
            return _get_none_value();
        }

        if (nullptr == m_data._object_ptr)
        {
            m_data._object_ptr = new (std::nothrow) json_object;
            if (nullptr == m_data._object_ptr)
            {
                return _get_none_value();
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
        if (this == &_get_none_value())
        {
            return _get_none_value();
        }

        if (!is_array())
        {
            return _get_none_value();
        }

        if (nullptr == m_data._array_ptr)
        {
            m_data._array_ptr = new (std::nothrow) json_array;
            if (nullptr == m_data._array_ptr)
            {
                return _get_none_value();
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

    bool json_value::is_bin() const
    {
        return json_type::json_type_bin == m_type;
    }

    json_bool json_value::as_bool() const
    {
        if (json_type::json_type_bool != m_type)
        {
            throw json_exception(__JSON_FUNCTION__);
        }

        return m_data._bool;
    }

    json_int json_value::as_int() const
    {
        if (!(json_type::json_type_int == m_type || json_type::json_type_uint == m_type))
        {
            throw json_exception(__JSON_FUNCTION__);
        }

        return m_data._int;
    }

    json_uint json_value::as_uint() const
    {
        if (!(json_type::json_type_int == m_type || json_type::json_type_uint == m_type))
        {
            throw json_exception(__JSON_FUNCTION__);
        }

        return m_data._uint;
    }

    json_float json_value::as_float() const
    {
        if (json_type::json_type_float != m_type)
        {
            throw json_exception(__JSON_FUNCTION__);
        }

        return m_data._float;
    }

    json_float json_value::as_number() const
    {
        if (!is_number())
        {
            throw json_exception(__JSON_FUNCTION__);
        }

        return m_data._float;
    }

    json_string& json_value::as_string() const
    {
        if (json_type::json_type_string != m_type)
        {
            throw json_exception(__JSON_FUNCTION__);
        }

        return *m_data._string_ptr;
    }

    json_object& json_value::as_object() const
    {
        if (json_type::json_type_object != m_type)
        {
            throw json_exception(__JSON_FUNCTION__);
        }

        return *m_data._object_ptr;
    }

    json_array& json_value::as_array() const
    {
        if (json_type::json_type_array != m_type)
        {
            throw json_exception(__JSON_FUNCTION__);
        }

        return *m_data._array_ptr;
    }

    json_bin& json_value::as_bin() const
    {
        if (json_type::json_type_bin != m_type)
        {
            throw json_exception(__JSON_FUNCTION__);
        }

        return *m_data._raw_ptr;
    }

    bool json_value::is_value(const _tstring& name) const
    {
        if (this == &_get_none_value())
        {
            return false;
        }

        if (is_object() && m_data._object_ptr)
        {
            auto it_find = m_data._object_ptr->find(name);
            if (m_data._object_ptr->end() != it_find)
            {
                return true;
            }
        }

        return false;
    }

    size_t json_value::count() const
    {
        if (this == &_get_none_value())
        {
            return 0;
        }

        if (is_array() && m_data._array_ptr)
        {
            return m_data._array_ptr->size();
        }

        if (is_object() && m_data._object_ptr)
        {
            return m_data._object_ptr->size();
        }

        return 0;
    }

    bool json_value::parse(const _tstring& text)
    {
        clear();
        const _tchar* end_ptr = nullptr;
        return _parse(text.c_str(), *this, &end_ptr);
    }

    bool json_value::parse_from_file(const _tstring& file_path)
    {
        std::string str_utf8;
        std::wstring str_utf16;
        _tstring read_text;

        clear();
        do
        {
            std::ifstream input_file(file_path, std::ios::binary | std::ios::in);
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

    bool json_value::parse_from_binary(uint8_t* data, size_t size)
    {
        clear();
        const uint8_t* data_end = data + size;
        return _parse_raw(data, &data_end, *this);
    }

    bool json_value::parse_from_binary_file(const _tstring& file_path)
    {
        clear();

        std::ifstream input_file(file_path, std::ios::binary | std::ios::in);
        if (!input_file.is_open())
        {
            return false;
        }

        input_file.seekg(0, std::ios::end);
        std::streamoff text_size = input_file.tellg();
        input_file.seekg(0, std::ios::beg);

        std::vector<uint8_t> text_buffer(text_size, 0);
        input_file.read((char*)&text_buffer[0], text_size);
        size_t byte_count = (size_t)input_file.gcount();
        input_file.close();

        const uint8_t* data_end = text_buffer.data() + text_buffer.size();
        return _parse_raw(text_buffer.data(), &data_end, *this);
    }

    _tstring json_value::dump(int indent/* = 0*/, bool flag_escape/* = false*/) const
    {
        _tstring result_text;
        std::vector<_tstring> indent_text({ _T("") });
        _dump(result_text, indent_text, 0, indent, flag_escape);
        return result_text;
    }

    bool json_value::dump_to_file(const _tstring& strPath, int indent/* = 0*/, bool flag_escape/* = false*/, json_encoding encoding/* = json_encoding::json_encoding_auto*/)
    {
        _tstring dump_text;
        std::vector<_tstring> indent_text({ _T("") });
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
                    val += std::move(_get_utf8_text_for_code_point(cp32));
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
                val += std::move(_get_utf8_text_for_code_point(cp32));
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
                append_str += _T(R"(\\)");
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

    void json_value::_dump_raw_int(std::vector<uint8_t>& append_buf, int64_t val) const
    {
        union _data_info
        {
            int64_t data;
            uint8_t bytes[sizeof(int64_t)];
        }data_info;

        data_info.data = val;
        int data_size = sizeof(int64_t);

        if (val <= INT8_MAX && val >= INT8_MIN)
        {
            append_buf.push_back(json_raw_type::raw_int8);
            data_size = sizeof(uint8_t);
        }
        else if (val <= INT16_MAX && val >= INT16_MIN)
        {
            append_buf.push_back(json_raw_type::raw_int16);
            data_size = sizeof(uint16_t);
        }
        else if (val <= INT32_MAX && val >= INT32_MIN)
        {
            append_buf.push_back(json_raw_type::raw_int32);
            data_size = sizeof(uint32_t);
        }
        else
        {
            append_buf.push_back(json_raw_type::raw_int64);
            data_size = sizeof(uint64_t);
        }

        for (int i = 0; i < data_size; i++)
        {
            append_buf.push_back(data_info.bytes[i]);
        }
    }

    void json_value::_dump_raw_uint(std::vector<uint8_t>& append_buf, uint64_t val) const
    {
        union _data_info
        {
            uint64_t data;
            uint8_t bytes[sizeof(uint64_t)];
        }data_info;

        data_info.data = val;
        int data_size = sizeof(uint64_t);

        if (val <= UINT8_MAX)
        {
            append_buf.push_back(json_raw_type::raw_uint8);
            data_size = sizeof(uint8_t);
        }
        else if (val <= UINT16_MAX)
        {
            append_buf.push_back(json_raw_type::raw_uint16);
            data_size = sizeof(uint16_t);
        }
        else if (val <= UINT32_MAX)
        {
            append_buf.push_back(json_raw_type::raw_uint32);
            data_size = sizeof(uint32_t);
        }
        else
        {
            append_buf.push_back(json_raw_type::raw_uint64);
            data_size = sizeof(uint64_t);
        }

        for (int i = 0; i < data_size; i++)
        {
            append_buf.push_back(data_info.bytes[i]);
        }
    }

    void json_value::_dump_raw_float(std::vector<uint8_t>& append_buf, double val) const
    {
        union _data_info
        {
            double data;
            uint8_t bytes[sizeof(double)];
        }data_info;

        data_info.data = val;

        for (int i = 0; i < sizeof(double); i++)
        {
            append_buf.push_back(data_info.bytes[i]);
        }
    }

    void json_value::_dump_raw_string(std::vector<uint8_t>& append_buf, const std::string& text) const
    {
        if (text.empty())
        {
            append_buf.push_back(json_raw_type::raw_string_empty);
        }
        else
        {
            int data_size = 0;
            union _data_info
            {
                uint32_t data;
                uint8_t bytes[sizeof(uint32_t)];
            }data_info;

            data_info.data = text.size();
            if (data_info.data <= UINT8_MAX)
            {
                append_buf.push_back(json_raw_type::raw_string8);
                data_size = sizeof(uint8_t);
            }
            else if (data_info.data <= UINT16_MAX)
            {
                append_buf.push_back(json_raw_type::raw_string16);
                data_size = sizeof(uint16_t);
            }
            else
            {
                append_buf.push_back(json_raw_type::raw_string32);
                data_size = sizeof(uint32_t);
            }

            for (int i = 0; i < data_size; i++)
            {
                append_buf.push_back(data_info.bytes[i]);
            }

            append_buf.insert(append_buf.end(), text.begin(), text.end());
        }
    }

    void json_value::_dump_raw_object(std::vector<uint8_t>& append_buf, const json_object& object) const
    {
        for (const auto& item : object)
        {
#ifdef _UNICODE
            _dump_raw_string(append_buf, _utf16_to_utf8(item.first));
#else
            _dump_raw_string(append_buf, item.first);
#endif
            item.second._dump_raw(append_buf);
        }
    }

    void json_value::_dump_raw_array(std::vector<uint8_t>& append_buf, const json_array& arrry) const
    {
        for (const auto& item : arrry)
        {
            item._dump_raw(append_buf);
        }
    }

    void json_value::_dump_raw_bin(std::vector<uint8_t>& append_buf, const json_bin& raw) const
    {
        if (raw.empty())
        {
            append_buf.push_back(json_raw_type::raw_bin_empty);
        }
        else
        {
            int data_size = 0;
            union _data_info
            {
                uint32_t data;
                uint8_t bytes[sizeof(uint32_t)];
            }data_info;

            data_info.data = raw.size();
            if (data_info.data <= UINT8_MAX)
            {
                append_buf.push_back(json_raw_type::raw_bin8);
                data_size = sizeof(uint8_t);
            }
            else if (data_info.data <= UINT16_MAX)
            {
                append_buf.push_back(json_raw_type::raw_bin16);
                data_size = sizeof(uint16_t);
            }
            else
            {
                append_buf.push_back(json_raw_type::raw_bin32);
                data_size = sizeof(uint32_t);
            }

            for (int i = 0; i < data_size; i++)
            {
                append_buf.push_back(data_info.bytes[i]);
            }

            append_buf.insert(append_buf.end(), raw.begin(), raw.end());
        }
    }

    void json_value::_dump_raw(std::vector<uint8_t>& append_buf) const
    {
        switch (m_type)
        {
        case json_type::json_type_null:
        {
            append_buf.push_back(json_raw_type::raw_null);
        }
        break;
        case json_type::json_type_bool:
        {
            if (m_data._bool)
            {
                append_buf.push_back(json_raw_type::raw_true);
            }
            else
            {
                append_buf.push_back(json_raw_type::raw_false);
            }
        }
        break;
        case json_type::json_type_int:
        {
            _dump_raw_int(append_buf, m_data._int);
        }
        break;
        case json_type::json_type_uint:
        {
            _dump_raw_uint(append_buf, m_data._uint);
        }
        break;
        case json_type::json_type_float:
        {
            append_buf.push_back(json_raw_type::raw_float);
            _dump_raw_float(append_buf, m_data._float);
        }
        break;
        case json_type::json_type_string:
        {
#ifdef _UNICODE
            _dump_raw_string(append_buf, _utf16_to_utf8(*m_data._string_ptr));
#else
            _dump_raw_string(append_buf, *m_data._string_ptr);
#endif
        }
        break;
        case json_type::json_type_object:
        {
            if (m_data._object_ptr && !m_data._object_ptr->empty())
            {
                append_buf.push_back(json_raw_type::raw_object_beg);
                _dump_raw_object(append_buf, *m_data._object_ptr);
                append_buf.push_back(json_raw_type::raw_object_end);
            }
            else
            {
                append_buf.push_back(json_raw_type::raw_object_empty);
            }
        }
        break;
        case json_type::json_type_array:
        {
            if (m_data._array_ptr && !m_data._array_ptr->empty())
            {
                append_buf.push_back(json_raw_type::raw_array_beg);
                _dump_raw_array(append_buf, *m_data._array_ptr);
                append_buf.push_back(json_raw_type::raw_array_end);
            }
            else
            {
                append_buf.push_back(json_raw_type::raw_array_empty);
            }
        }
        break;
        case json_type::json_type_bin:
        {
            if (m_data._raw_ptr && !m_data._raw_ptr->empty())
            {
                _dump_raw_bin(append_buf, *m_data._raw_ptr);
            }
            else
            {
                append_buf.push_back(json_raw_type::raw_bin_empty);
            }
        }
        break;
        }
    }

    bool json_value::_parse_raw_string(const uint8_t* data_ptr, const uint8_t** end_ptr, json_value& val)
    {
        json_raw_type type = (json_raw_type)*data_ptr;
        const uint8_t* date_value_ptr = data_ptr + sizeof(json_raw_type);
        const uint8_t* data_next_ptr = data_ptr;
        std::string str_value;

        switch (*data_ptr)
        {
        case json_raw_type::raw_string_empty:
            data_next_ptr += sizeof(json_raw_type);
            break;
        case json_raw_type::raw_string8:
            str_value = std::string((const char*)(date_value_ptr + sizeof(uint8_t)), *(uint8_t*)date_value_ptr);
            data_next_ptr = date_value_ptr + sizeof(uint8_t) + *(uint8_t*)date_value_ptr;
            break;
        case json_raw_type::raw_string16:
            str_value = std::string((const char*)(date_value_ptr + sizeof(uint16_t)), *(uint16_t*)date_value_ptr);
            data_next_ptr = date_value_ptr + sizeof(uint16_t) + *(uint16_t*)date_value_ptr;
            break;
        case json_raw_type::raw_string32:
            str_value = std::string((const char*)(date_value_ptr + sizeof(uint32_t)), *(uint32_t*)date_value_ptr);
            data_next_ptr = date_value_ptr + sizeof(uint32_t) + *(uint32_t*)date_value_ptr;
            break;
        }

#ifdef _UNICODE
        val = _utf8_to_utf16(str_value);
#else
        val = str_value;
#endif

        if (end_ptr)
        {
            *end_ptr = data_next_ptr;
        }

        return true;
    }

    bool json_value::_parse_raw_object(const uint8_t* data_ptr, const uint8_t** end_ptr, json_value& val)
    {
        val._reset_type(json_type::json_type_object);

        const uint8_t* data_next_ptr = data_ptr;
        while (true)
        {
            std::string key_name;
            json_value key_value;

            if (json_raw_type::raw_object_end == *data_next_ptr)
            {
                data_next_ptr++;
                break;
            }

            const uint8_t* value_ptr = data_next_ptr + sizeof(json_raw_type);
            size_t value_size = 0;
            switch (*data_next_ptr)
            {
            case json_raw_type::raw_string8:
                value_size = *(uint8_t*)value_ptr;
                key_name = std::string((const char*)(value_ptr + sizeof(uint8_t)), value_size);
                data_next_ptr = (value_ptr + sizeof(uint8_t)) + value_size;
                break;
            case json_raw_type::raw_string16:
                value_size = *(uint16_t*)value_ptr;
                key_name = std::string((const char*)(value_ptr + sizeof(uint16_t)), value_size);
                data_next_ptr = (value_ptr + sizeof(uint16_t)) + value_size;
                break;
            case json_raw_type::raw_string32:
                value_size = *(uint32_t*)value_ptr;
                key_name = std::string((const char*)(value_ptr + sizeof(uint32_t)), value_size);
                data_next_ptr = (value_ptr + sizeof(uint32_t)) + value_size;
                break;
            default:
                return false;
            }

            if (key_name.empty())
            {
                return false;
            }

            if (!_parse_raw(data_next_ptr, &data_next_ptr, key_value))
            {
                return false;
            }

#ifdef _UNICODE
            val[_utf8_to_utf16(key_name)] = key_value;
#else
            val[key_name] = key_value;
#endif
        }

        if (end_ptr)
        {
            *end_ptr = data_next_ptr;
        }

        return true;
    }

    bool json_value::_parse_raw_array(const uint8_t* data_ptr, const uint8_t** end_ptr, json_value& val)
    {
        const uint8_t* data_next_ptr = data_ptr;
        val._reset_type(json_type::json_type_array);

        while (true)
        {
            if (json_raw_type::raw_array_end == *data_next_ptr)
            {
                data_next_ptr++;
                break;
            }

            json_value value;
            if (!_parse_raw(data_next_ptr, &data_next_ptr, value))
            {
                return false;
            }

            val.m_data._array_ptr->emplace_back(std::move(value));
        }

        if (end_ptr)
        {
            *end_ptr = data_next_ptr;
        }

        return true;
    }

    bool json_value::_parse_raw_bin(const uint8_t* data_ptr, const uint8_t** end_ptr, json_value& val)
    {
        json_raw_type type = (json_raw_type)*data_ptr;
        const uint8_t* date_value_ptr = data_ptr + sizeof(json_raw_type);
        const uint8_t* data_next_ptr = data_ptr;

        json_bin raw;

        switch (*data_ptr)
        {
        case json_raw_type::raw_bin_empty:
            val = json_bin();
            data_next_ptr += sizeof(json_raw_type);
            break;
        case json_raw_type::raw_bin8:
            raw.assign(date_value_ptr + sizeof(uint8_t), date_value_ptr + sizeof(uint8_t) + *(uint8_t*)date_value_ptr);
            val = std::move(raw);
            data_next_ptr = date_value_ptr + sizeof(uint8_t) + *(uint8_t*)date_value_ptr;
            break;
        case json_raw_type::raw_bin16:
            raw.assign(date_value_ptr + sizeof(uint16_t), date_value_ptr + sizeof(uint16_t) + *(uint16_t*)date_value_ptr);
            val = std::move(raw);
            data_next_ptr = date_value_ptr + sizeof(uint16_t) + *(uint16_t*)date_value_ptr;
            break;
        case json_raw_type::raw_bin32:
            raw.assign(date_value_ptr + sizeof(uint32_t), date_value_ptr + sizeof(uint32_t) + *(uint32_t*)date_value_ptr);
            val = std::move(raw);
            data_next_ptr = date_value_ptr + sizeof(uint32_t) + *(uint32_t*)date_value_ptr;
            break;
        }

        if (end_ptr)
        {
            *end_ptr = data_next_ptr;
        }

        return true;
    }

    bool json_value::_parse_raw(const uint8_t* data_ptr, const uint8_t** end_ptr, json_value& val)
    {
        const uint8_t* date_value_ptr = data_ptr + sizeof(json_raw_type);
        const uint8_t* data_next_ptr = data_ptr;
        bool parse_result = true;

        switch (*data_next_ptr)
        {
        case json_raw_type::raw_null:
            val = json_type::json_type_null;
            data_next_ptr += sizeof(json_raw_type);
            break;
        case json_raw_type::raw_false:
            val = false;
            data_next_ptr += sizeof(json_raw_type);
            break;
        case json_raw_type::raw_true:
            val = true;
            data_next_ptr += sizeof(json_raw_type);
            break;
        case json_raw_type::raw_int8:
            val = *(int8_t*)date_value_ptr;
            data_next_ptr += sizeof(json_raw_type) + sizeof(int8_t);
            break;
        case json_raw_type::raw_int16:
            val = *(int16_t*)date_value_ptr;
            data_next_ptr += sizeof(json_raw_type) + sizeof(int16_t);
            break;
        case json_raw_type::raw_int32:
            val = *(int32_t*)date_value_ptr;
            data_next_ptr += sizeof(json_raw_type) + sizeof(int32_t);
            break;
        case json_raw_type::raw_int64:
            val = *(int64_t*)date_value_ptr;
            data_next_ptr += sizeof(json_raw_type) + sizeof(int64_t);
            break;
        case json_raw_type::raw_uint8:
            val = *(uint8_t*)date_value_ptr;
            data_next_ptr += sizeof(json_raw_type) + sizeof(uint8_t);
            break;
        case json_raw_type::raw_uint16:
            val = *(uint16_t*)date_value_ptr;
            data_next_ptr += sizeof(json_raw_type) + sizeof(uint16_t);
            break;
        case json_raw_type::raw_uint32:
            val = *(uint32_t*)date_value_ptr;
            data_next_ptr += sizeof(json_raw_type) + sizeof(uint32_t);
            break;
        case json_raw_type::raw_uint64:
            val = *(uint64_t*)date_value_ptr;
            data_next_ptr += sizeof(json_raw_type) + sizeof(uint64_t);
            break;
        case json_raw_type::raw_float:
            val = *(double*)date_value_ptr;
            data_next_ptr += sizeof(json_raw_type) + sizeof(double);
            break;
        case json_raw_type::raw_string_empty:
        case json_raw_type::raw_string8:
        case json_raw_type::raw_string16:
        case json_raw_type::raw_string32:
            parse_result = _parse_raw_string(data_next_ptr, &data_next_ptr, val);
            break;
        case json_raw_type::raw_bin_empty:
        case json_raw_type::raw_bin8:
        case json_raw_type::raw_bin16:
        case json_raw_type::raw_bin32:
            parse_result = _parse_raw_bin(data_next_ptr, &data_next_ptr, val);
            break;
        case json_raw_type::raw_object_empty:
            val._reset_type(json_type::json_type_object);
            data_next_ptr += sizeof(json_raw_type);
            break;
        case json_raw_type::raw_object_beg:
            parse_result = _parse_raw_object(date_value_ptr, &data_next_ptr, val);
            break;
        case json_raw_type::raw_object_end:
            data_next_ptr += sizeof(json_raw_type);
            break;
        case json_raw_type::raw_array_empty:
            val._reset_type(json_type::json_type_array);
            data_next_ptr += sizeof(json_raw_type);
            break;
        case json_raw_type::raw_array_beg:
            parse_result = _parse_raw_array(date_value_ptr, &data_next_ptr, val);
            break;
        case json_raw_type::raw_array_end:
            data_next_ptr += sizeof(json_raw_type);
            break;
        default:
            return false;
        }

        if (!parse_result)
        {
            return parse_result;
        }

        if (end_ptr)
        {
            *end_ptr = data_next_ptr;
        }

        return true;
    }

    std::vector<uint8_t> json_value::dump_to_binary()
    {
        std::vector<uint8_t> result;
        _dump_raw(result);
        return result;
    }

    bool json_value::dump_to_binary_file(const _tstring& strPath)
    {
        std::vector<uint8_t> result;
        _dump_raw(result);

        std::ofstream output_file(strPath, std::ios::binary | std::ios::out);
        if (!output_file.is_open())
        {
            return false;
        }

        output_file.write((char*)result.data(), result.size());
        output_file.close();

        return true;
    }

    json_value& json_value::_get_none_value()
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

        data_ptr = _skip_whitespace(data_ptr);

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
            _reset_type(json_type::json_type_null);
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
        else if (cp32 >= 0x00000080 && cp32 <= 0x000007FF)
        {
            text_buffer[0] = ((cp32 >> 6) & 0x1F) | 0xC0;
            text_buffer[1] = ((cp32 & 0x3F)) | 0x80;
            text_buffer[2] = 0;
        }
        // 3bytes 1110xxxx 10xxxxxx 10xxxxxx
        else if (cp32 >= 0x00000800 && cp32 <= 0x0000FFFF)
        {
            text_buffer[0] = ((cp32 >> 12) & 0x0F) | 0xE0;
            text_buffer[1] = ((cp32 >> 6) & 0x3F) | 0x80;
            text_buffer[2] = ((cp32 & 0x3F)) | 0x80;
            text_buffer[3] = 0;
        }
        // 4bytes 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        else if (cp32 >= 0x00010000 && cp32 <= 0x001FFFFF)
        {
            text_buffer[0] = ((cp32 >> 18) & 0x07) | 0xF0;
            text_buffer[1] = ((cp32 >> 12) & 0x3F) | 0x80;
            text_buffer[2] = ((cp32 >> 6) & 0x3F) | 0x80;
            text_buffer[3] = ((cp32 & 0x3F)) | 0x80;
            text_buffer[4] = 0;
        }
        // 5bytes 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        else if (cp32 >= 0x00200000 && cp32 <= 0x03FFFFFF)
        {
            text_buffer[0] = ((cp32 >> 24) & 0x03) | 0xF8;
            text_buffer[1] = ((cp32 >> 18) & 0x3F) | 0x80;
            text_buffer[2] = ((cp32 >> 12) & 0x3F) | 0x80;
            text_buffer[3] = ((cp32 >> 6) & 0x3F) | 0x80;
            text_buffer[4] = ((cp32 & 0x3F)) | 0x80;
            text_buffer[5] = 0;
        }
        // 6bytes 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        else if (cp32 >= 0x04000000 && cp32 <= 0x7FFFFFFF)
        {
            text_buffer[0] = ((cp32 >> 30) & 0x01) | 0xFC;
            text_buffer[1] = ((cp32 >> 24) & 0x3F) | 0x80;
            text_buffer[2] = ((cp32 >> 18) & 0x3F) | 0x80;
            text_buffer[3] = ((cp32 >> 12) & 0x3F) | 0x80;
            text_buffer[4] = ((cp32 >> 6) & 0x3F) | 0x80;
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
                    text_out_utf8 += std::move(_get_utf8_text_for_code_point(cp32));
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
                    text_out_utf8 += std::move(_get_utf8_text_for_code_point(cp32));
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

    std::string _utf16_to_utf8(const std::wstring utf16)
    {
        std::string str_utf8;
        std::wstring str_utf16;
        int32_t utf8_length = _utf16_to_utf8(utf16.data(), utf16.size() * sizeof(wchar_t), &str_utf8, &str_utf16);
        return str_utf8;
    }

    std::wstring _utf8_to_utf16(const std::string utf8)
    {
        std::string str_utf8;
        std::wstring str_utf16;
        int32_t utf16_length = _utf8_to_utf16(utf8.data(), utf8.size() * sizeof(wchar_t), &str_utf8, &str_utf16);
        return str_utf16;
    }

#ifdef _WIN32

    static std::string _WStrToMultiStr(uint32_t CodePage, const std::wstring& str)
    {
        int cbMultiByte = ::WideCharToMultiByte(CodePage, 0, str.c_str(), -1, NULL, 0, NULL, 0);
        std::string strResult(cbMultiByte, 0);
        size_t nConverted = ::WideCharToMultiByte(CodePage, 0, str.c_str(), (int)str.size(), &strResult[0], (int)strResult.size(), NULL, NULL);
        strResult.resize(nConverted);
        return strResult;
    }

    static std::wstring _MultiStrToWStr(uint32_t CodePage, const std::string& str)
    {
        int cchWideChar = ::MultiByteToWideChar(CodePage, 0, str.c_str(), -1, NULL, NULL);
        std::wstring strResult(cchWideChar, 0);
        size_t nConverted = ::MultiByteToWideChar(CodePage, 0, str.c_str(), (int)str.size(), &strResult[0], (int)strResult.size());
        strResult.resize(nConverted);
        return strResult;
    }

    std::wstring AStrToWStr(const std::string& str)
    {
        return _MultiStrToWStr(CP_ACP, str);
    }

    std::string AStrToU8Str(const std::string& str)
    {
        return _WStrToMultiStr(CP_UTF8, _MultiStrToWStr(CP_ACP, str));
    }

    _tstring AStrToTStr(const std::string& str)
    {
#ifdef _UNICODE
        return _MultiStrToWStr(CP_ACP, str);
#else
        return str;
#endif
    }

    std::string WStrToAStr(const std::wstring& str)
    {
        return _WStrToMultiStr(CP_ACP, str);
    }

    std::string WStrToU8Str(const std::wstring& str)
    {
        return _WStrToMultiStr(CP_UTF8, str);
    }

    _tstring WStrToTStr(const std::wstring& str)
    {
#ifdef _UNICODE
        return str;
#else
        return _WStrToMultiStr(CP_ACP, str);
#endif
    }

    std::wstring U8StrToWStr(const std::string& str)
    {
        return _MultiStrToWStr(CP_UTF8, str);
    }

    std::string U8StrToAStr(const std::string& str)
    {
        return _WStrToMultiStr(CP_ACP, U8StrToWStr(str));
    }

    _tstring U8StrToTStr(const std::string& str)
    {
#ifdef _UNICODE
        return _MultiStrToWStr(CP_UTF8, str);
#else
        return _WStrToMultiStr(CP_ACP, U8StrToWStr(str));
#endif
    }

    std::string TStrToU8Str(const _tstring& str)
    {
#ifdef _UNICODE
        return _WStrToMultiStr(CP_UTF8, str);
#else
        return _WStrToMultiStr(CP_UTF8, _MultiStrToWStr(CP_ACP, str));
#endif
    }

    std::string TStrToAStr(const _tstring& str)
    {
#ifdef _UNICODE
        return _WStrToMultiStr(CP_ACP, str);
#else
        return str;
#endif
    }

    std::wstring TStrToWStr(const _tstring& str)
    {
#ifdef _UNICODE
        return str;
#else
        return _MultiStrToWStr(CP_ACP, str);
#endif
    }

#endif

}