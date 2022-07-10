# CompactJSON - Single include JSON C++17 library

## Type system

### Primitive types

JSON string type represented as ``std::string``, number type as ``double`` for rational numbers and ``int64_t`` for integers and ``bool`` for booleans.

### Container

Array and object not available outside JSONBase class. Inside arrays stored as ``std::vector``, objects as ``std::map``.

## JSON objects creation

```cpp
/*
    create this json:
    {
        "used includes": ["sstream", "map", "string"]
    }
*/
//can been created json using implicit constuctors
JSON j1 = {
    {"used includes", {"sstream", "map", "string"}}
};
//or [] operator
JSON j2;
j2["enable_synch"] = false;
/*
    now j2 same as
    {
        "enable_synch": false
    }
*/ 
```

## Input and output streams

```cpp
std::cout << j2 << std::endl;//print {"enable_synch":false}
if(in.is_open()) std::ifstream in("example.json");
in >> j2;//scan json object to j2 from example.json file
```

## To string and from string conversions

```cpp
std::string s1 = j1.to_string();
/*
    now s1 == "{"used includes":["sstream","map","string"]}"
    json converted without spaces because by defualt tab_size arg is -1
    (in ostream operator << tab size is also -1)
*/
std::string s2 = j1.to_string(4);//pass tab_size 4
/*
    now s2 is
    {
        "used includes": [
            "sstream", 
            "map", 
            "string"
        ]
    }
*/

//parse json back from string
auto j3 = JSON::from_string(s2);
```

## Data access

```cpp
/*
    to check contained type you can use .is_***() metods for 
    array, object, number, integer etc.
*/
if(j3.is_object() && j3.contains("used includes")) {
    //print array "used includes".
    std::cout << j3["used includes"] << std::endl;
    //printed ["sstream","map","string"]

    auto& j4 = j3["used includes"];
    //j4 type is JSON& - link on "used includes" value

    //print first elemebt of the array if exist
    if(j4.is_array() && j4.array_size() > 0) {
        if(j4[0].is_string())
            std::cout << j4[0].get<std::string>();
        //get<std::string>() return string from json
    }
}
JSON j5 = 15;
if(j5.is_integer())
    auto i = j5.get<int>();
```

For more features check example.cpp & json.hpp
