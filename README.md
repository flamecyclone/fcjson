# fcjson

## Introduction
Minimal JSON for C++

## Software Architecture
C++11

## Installation Guide

Add fcjson.h and fcjson.cpp to your project and reference them accordingly.

## Usage Instructions

  ```c++
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
  ```

  

- ### Parse string/dump string

  ```c++
  fcjson::json_value val;
  
  val.parse(R"({"name":"FlameCyclone","age":30})");
  std::string strJson = val.dump(4, true);
  std::cout << strJson << std::endl;
  
  // 访问数组
  val["array"] = fcjson::json_type::json_type_array;
  auto& array = val["array"];
  for (int i = 0; i < 5; i++)
  {
      array[i] = i;
  }
  
  // 删除数组元素
  array.remove(4);
  
  // 访问对象
  val["object"] = fcjson::json_type::json_type_object;
  auto& object = val["object"];
  for (int i = 0; i < 5; i++)
  {
      object[std::to_string(i)] = i;
  }
  
  // 删除对象元素
  object.remove("1");
  
  //赋值
  val["hobby"] = "C++";
  val.remove("object");
  val["hobby"] = nullptr;
  
  std::cout << val.dump(4, true) << std::endl;
  ```

  

- ### Parse/Dump

  ```c++
  fcjson::json_value val;
  val.parse_from_file("data.json");
  val.dump_to_file("dump.json", 4);
  ```

  

- ### Example

  ```c++
  #include <iostream>
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
      setlocale(LC_ALL, "en_US.UTF-8");
  
      // 构造 JSON 对象
      std::cout << "构造 JSON 对象" << std::endl;
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
  
          // 序列化(不转义UNICODE字符)
          std::cout << val.dump(4, false) << std::endl;
  
          // 序列化(转义UNICODE字符)
          std::cout << val.dump(4, true) << std::endl;
      }
  
      // 赋值操作
      std::cout << std::endl;
      std::cout << "赋值操作" << std::endl;
      {
          fcjson::json_value val;
          val = fcjson::json_array{ 1,2,3,4,5,6,7,8,9,0 };
          std::cout << "count: " << val.count() << std::endl;
          std::cout << "type: " << val.get_type_name() << std::endl;
          std::cout << val.dump(4, false) << std::endl;
  
          val = fcjson::json_object{{ "name", "我是地球🌍" }, { "age", 30 }};
          std::cout << "count: " << val.count() << std::endl;
          std::cout << "type: " << val.get_type_name() << std::endl;
          std::cout << val.dump(4, false) << std::endl;
      }
  
      // 解析字符串/转储字符串
      std::cout << std::endl;
      std::cout << "解析字符串/转储字符串" << std::endl;
      {
          fcjson::json_value val;
  
          val.parse(R"({"name":"FlameCyclone","age":30})");
          std::string strJson = val.dump(4, true);
          std::cout << strJson << std::endl;
  
          // 访问数组
          val["array"] = fcjson::json_type::json_type_array;
          auto& array = val["array"];
          for (int i = 0; i < 5; i++)
          {
              array[i] = i;
          }
  
          // 删除数组元素
          array.remove(4);
  
          // 访问对象
          val["object"] = fcjson::json_type::json_type_object;
          auto& object = val["object"];
          for (int i = 0; i < 5; i++)
          {
              object[std::to_string(i)] = i;
          }
  
          // 删除对象元素
          object.remove("1");
  
          //赋值
          val["hobby"] = "C++";
          val.remove("object");
          val["hobby"] = nullptr;
  
          std::cout << val.dump(4, true) << std::endl;
      }
  
      // 解析文件/转储文件
      std::cout << std::endl;
      std::cout << "解析文件/转储文件" << std::endl;
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
  
      // 性能测试
      size_t nCount = count;
      clock_t timeBegin = clock();
      clock_t timeEnd = clock();
  
      std::cout << std::endl;
      std::cout << "性能测试" << std::endl;
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
  
  ```
  
- ### 性能

  解析某个 2.7MB json文件对比

  ```c++
  fcjson
  fcjson parse cost time: 135ms
  fcjson dump cost time: 39ms
  fcjson size: 2850256
  
  nlohmann
  nlohmann parse cost time: 97ms
  nlohmann dump cost time: 33ms
  nlohmann size: 2850256
  
  nomango
  nomango parse cost time: 767ms
  nomango dump cost time: 442ms
  nomango size: 2850256
  ```

  

## 参与贡献

- FlameCyclone

