#include <iostream>
#include <tchar.h>
#include <locale>
#include <string>
#include <fstream>
#include "fcjson.h"

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
    setlocale(LC_ALL, "");

    size_t nCount = count;
    clock_t timeBegin = clock();
    clock_t timeEnd = clock();

    {
        // 构造json
        fcjson::json_value fcjson = fcjson::json_object{
            { "null", nullptr},
            { "bool_false", false },
            { "bool_true", true },
            { "int_min", INT64_MIN },
            { "int_max", INT64_MAX },
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

        // 序列化
        std::string strJson = fcjson.dump(4, true);
        std::cout << strJson << std::endl;
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
    size_t nByteSize = (size_t)inputFile.gcount();
    inputFile.close();

    // 性能测试
    while (true)
    {
        {
            fcjson::json_value fcjson;
            std::cout << "fcjson" << std::endl;
            timeBegin = clock();
            for (int i = 0; i < nCount; i++)
            {
                fcjson.parse(strBuffer);
            }
            timeEnd = clock();
            std::cout << "parse cost time: " << timeEnd - timeBegin << std::endl;

            timeBegin = clock();
            std::string strDump;
            for (int i = 0; i < nCount; i++)
            {
                strDump = fcjson.dump(dump_indent);
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
