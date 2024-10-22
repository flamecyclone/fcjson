#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdint.h>
#include <string>
#include <vector>
#include <map>

// VS 将执行字符集指定为 UTF-8
// 项目属性页 -> 配置属性 -> C/C++ -> 命令行 -> 其他选项(D) 
// 加入 /execution-charset:utf-8 或/utf-8
//

// 此编译器指令在 Visual Studio 2015 Update 2 及更高版本中已过时
// #pragma execution_character_set("utf-8")

// JSON规范: https://www.json.org/json-zh.html

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

// JSON 解析
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

    // 异常
    class json_exception
    {
    public:
        json_exception(const _tstring& msg) :
            m_Message(msg)
        {
        }

        _tstring get_message() const
        {
            return m_Message;
        }

        _tstring get_text_pos() const
        {
            return m_Message;
        }

    private:
        _tstring    m_Message;      // 提示
    };

    // JSON 数据类型
    enum json_type
    {
        json_type_null,              // 空值       null
        json_type_string,            // 字符串     "FlameCyclone"
        json_type_int,               // 有符号整数 正数: 0 - 9223372036854775807 负数: -1 - (9223372036854775807 - 1)
        json_type_uint,              // 无符号整数 0 - 18446744073709551615
        json_type_float,             // 浮点数     3.141592653589793
        json_type_bool,              // 布尔       true 或 false
        json_type_object,            // 对象       [128,256,512,1204,"string",{"name":"FlameCyclone"}]
        json_type_array,             // 数组       {"name":"FlameCyclone"}
    };

    // JSON 编码
    enum json_encoding
    {
        json_encoding_auto,              // 自动
        json_encoding_utf8,              // Utf8
        json_encoding_utf16,             // Utf16编码
    };

    // JSON 值类
    class json_value
    {
    public:

        // 构造
        json_value();
        json_value(json_type type);
        json_value(nullptr_t val);
        json_value(json_bool val);
        json_value(int32_t val);
        json_value(uint32_t val);
        json_value(int64_t val);
        json_value(uint64_t val);
        json_value(json_float val);
        json_value(const _tchar* val);
        json_value(const json_string& val);
        json_value(const json_object& val);
        json_value(const json_array& val);
        json_value(const json_value& r);
        json_value(json_value&& r) noexcept;
        json_value(json_string&& val);
        json_value(json_object&& val);
        json_value(json_array&& val);

        // 运算符重载
        json_value& operator = (json_type type);
        json_value& operator = (json_bool val);
        json_value& operator = (int32_t val);
        json_value& operator = (uint32_t val);
        json_value& operator = (int64_t val);
        json_value& operator = (uint64_t val);
        json_value& operator = (json_float val);
        json_value& operator = (const _tchar* val);
        json_value& operator = (const json_string& val);
        json_value& operator = (const json_object& val);
        json_value& operator = (const json_array& val);
        json_value& operator = (const json_value& r);
        json_value& operator = (json_string&& val);
        json_value& operator = (json_object&& val);
        json_value& operator = (json_array&& val);
        json_value& operator = (json_value&& r) noexcept;
        json_value& operator [] (const _tstring& name);
        json_value& operator [] (const size_t index);

        // 清空
        void clear();

        ~json_value();

        // 检查与类型判断
        json_type get_type() const;
        _tstring get_type_name() const;

        // 类型判断
        bool is_null() const;
        bool is_bool() const;
        bool is_int() const;
        bool is_float() const;
        bool is_string() const;
        bool is_object() const;
        bool is_array() const;

        // 获取数据
        json_bool as_bool() const;
        json_int as_int() const;
        json_uint as_uint() const;
        json_float as_float() const;
        json_string as_string() const;
        json_object as_object() const;
        json_array as_array() const;

        // 
        // @brief: 获取本身或子项计数
        // @param: name                 子项(空字符串: 本身对象或数组的计数 非空: 对象子项的计数)
        // @ret: size_t                 计数, 非数组或对象时返回1, 不存在返回0
        size_t count(const _tstring& name = _T("")) const;

        // 
        // @brief: 从文本解析(仅支持 UTF8 或 UTF16编码的文本)
        // @param: text                 文本内容
        // @ret: json_value             JSON值
        json_value parse(const _tstring& text);

        // 
        // @brief: 从文件解析(仅支持 UTF8 或 UTF16编码的文件)
        // @param: file_path            文件路径
        // @ret: json_value             JSON值
        json_value parse_from_file(const _tstring& file_path);

        // 
        // @brief: 转储为文本
        // @param: indent               缩进空格数量
        // @param: flag_escape          是否转义非Ascii字符
        // @ret: std::wstring           转储文本
        _tstring dump(int indent = 0, bool flag_escape = false) const;

        // 
        // @brief: 转储到文件
        // @param: file_path            文件路径
        // @param: indent               缩进空格数量
        // @param: enc                  Unicode编码类型(UTF8或UTF16)
        // @param: flag_escape          是否转义非Ascii字符
        // @ret: std::wstring           转储文本
        bool dump_to_file(const _tstring& file_path, int indent = 0, bool flag_escape = false, json_encoding enc = json_encoding::json_encoding_auto);

    private:

        inline void reset_type(json_type type);

        // 解析
        json_value _parse(const _tchar* pData, const _tchar** pEnd);
        bool _parse_number(const _tchar* pData, json_value& val, const _tchar** pEnd);
        bool _parse_unicode(const _tchar* pData, _tstring& val, const _tchar** pEnd);
        bool _parse_string(const _tchar* pData, _tstring& val, const _tchar** pEnd);
        bool _parse_object(const _tchar* pData, json_value& val, const _tchar** pEnd);
        bool _parse_array(const _tchar* pData, json_value& val, const _tchar** pEnd);
        bool _parse_value(const _tchar* pData, json_value& val, const _tchar** pEnd);

        // 转储
        void _dump(_tstring& append_buf, int depth, int indent, bool flag_escape) const;
        void _dump_int(_tstring& append_buf, int64_t val) const;
        void _dump_uint(_tstring& append_buf, uint64_t val) const;
        void _dump_float(_tstring& append_buf, double val) const;
        void _dump_string(_tstring& append_buf, const _tstring& text, bool flag_escape) const;
        void _dump_object(_tstring& append_buf, int depth, int indent, bool flag_escape) const;
        void _dump_array(_tstring& append_buf, int depth, int indent, bool flag_escape) const;

    private:

        // JSON 数据
        union json_data
        {
            json_bool    _bool;             // 布尔类型          bool
            json_int     _int;              // 有符号整数        int64_t
            json_uint    _uint;             // 无符号整数        uint64_t
            json_float   _float;            // 浮点数            double
            json_string* _string_ptr;       // 字符串指针        std::string
            json_object* _object_ptr;       // 对象类型指针      std::map
            json_array*  _array_ptr;        // 数组类型指针      std::vector
        }m_data;                            // 数据

        json_type    m_type;                // 类型(表示当前数据所属类型)
    };
}