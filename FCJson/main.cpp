#include <iostream>
#include <locale>
#include <string>
#include <fstream>
#include "fcjson/fcjson.h"

#pragma execution_character_set("utf-8")

#if  0
#define TEST_JSON_FILE  "data.json"
#else
#define TEST_JSON_FILE  "city_4.json"
#endif

int count = 1;
int dump_indent = 4;

int main()
{
    setlocale(LC_ALL, "en_US.UTF-8");

    // Construct a JSON object
    std::cout << "Construct a JSON object" << std::endl;
    {
        fcjson::json_value val = fcjson::json_object{
            { "null", nullptr},
            { "bool_false", false },
            { "bool_true", true },
            { "int_min", INT64_MIN },
            { "int_max", INT64_MAX },
            { "uint_max", UINT64_MAX },
            { "float", 3.1415926535 },
            { "object", fcjson::json_object{
                    { "name", "我是地球🌍" },
                    { "age", 30 }
                },
            },
            { "array", fcjson::json_array{
                nullptr,
                false, true, INT64_MIN, INT64_MAX, 3.1415926535
                }
            }
        };

        // Serialize (without escaping UNICODE characters)
        std::cout << val.dump(4, false) << std::endl;

        // Serialization (with escaping UNICODE characters)
        std::cout << val.dump(4, true) << std::endl;
    }

    // Assignment Operation
    std::cout << std::endl;
    std::cout << "Assignment Operation" << std::endl;
    {
        fcjson::json_value val;

        val = fcjson::json_array{ 1,2,3,4,5,6,7,8,9,0 };
        std::cout << "count: " << val.count() << std::endl;
        std::cout << "type: " << val.type_name() << std::endl;
        std::cout << val.dump(4, false) << std::endl;

        val = fcjson::json_object{ { "name", "我是地球🌍" }, { "age", 30 } };
        std::cout << "count: " << val.count() << std::endl;
        std::cout << "type: " << val.type_name() << std::endl;
        std::cout << val.dump(4, false) << std::endl;
    }

    // Parse String / Dump String
    std::cout << std::endl;
    std::cout << "Parse String / Dump String" << std::endl;
    {
        fcjson::json_value val;

        val.parse(R"({"name":"FlameCyclone","age":30})");
        std::string strJson = val.dump(4, true);
        std::cout << strJson << std::endl;

        // Access Array
        val["array"] = fcjson::json_type::json_type_array;
        auto& array = val["array"];
        for (int i = 0; i < 5; i++)
        {
            array[i] = i;
        }

        // Delete Array Element
        array.remove(4);

        // Access Object
        val["object"] = fcjson::json_type::json_type_object;
        auto& object = val["object"];
        for (int i = 0; i < 5; i++)
        {
            object[std::to_string(i)] = i;
        }

        // Delete Object Element
        object.remove("");

        // Assignment
        val["hobby"] = "C++";
        val.remove("object");
        val["hobby"] = nullptr;

        std::cout << val.dump(4, true) << std::endl;
    }

    // Parse File / Dump File
    std::cout << std::endl;
    std::cout << "Parse File / Dump File" << std::endl;
    {
        fcjson::json_value val;
        val.parse_from_file("data.json");
        val.dump_to_file("dump.json", 4);
    }

    std::ifstream inputFile(TEST_JSON_FILE, std::ios::binary | std::ios::in);
    if (!inputFile.is_open())
    {
        return -1;
    }

    inputFile.seekg(0, std::ios::end);
    std::streamoff nSize = inputFile.tellg();
    inputFile.seekg(0, std::ios::beg);

    std::string strBuffer(nSize, 0);
    inputFile.read((char*)&strBuffer[0], nSize);
    inputFile.close();

    // Performance Testing
    size_t nCount = count;
    clock_t timeBegin = clock();
    clock_t timeEnd = clock();

    std::cout << std::endl;
    std::cout << "Performance Testing" << std::endl;
    while (true)
    {
        {
            fcjson::json_value val;
            val.parse_from_file("data.json");

            timeBegin = clock();
            for (int i = 0; i < nCount; i++)
            {
                val.parse(strBuffer);
            }
            timeEnd = clock();
            std::cout << "parse cost time: " << timeEnd - timeBegin << std::endl;

            timeBegin = clock();
            std::string strDump;
            for (int i = 0; i < nCount; i++)
            {
                strDump = val.dump(dump_indent);
            }
            timeEnd = clock();
            std::cout << "dump cost time: " << timeEnd - timeBegin << std::endl;
            std::cout << "dump text size: " << strDump.size() << std::endl;

            {
                std::ofstream outputFile("dump_fcjson.json", std::ios::binary | std::ios::out);
                if (outputFile.is_open())
                {
                    outputFile.write(strDump.data(), strDump.size());
                }
            }
            std::cout << std::endl;
        }

        system("pause");
    }

    return 0;
}
