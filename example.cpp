#include <iostream>
#include <fstream>
#include "json.hpp"
using CompactJSON::JSON;
int main() {

	JSON j1 = {
	{"integer", 42},
	{"float", 2.6},
	{"bool", false},
	{"string", "CompactJSON"},
	{"null", nullptr},
	{"array", {
		1, 
		true, 
		-8, 
		3.4, 
		"str", 
		{"subarray", 34., nullptr}
	}},
	{"object", {
		{"value1", 1426}, 
		{"value2", "everything"}
	}}
	};

	//or using overloaded [] opertator:
	JSON j2;//empty JSON

	//insert 42 (JSON-number) value in object whith key
	j2["integer"] = 42;
	j2["float"] = 2.6;
	j2["bool"] = false;
	j2["string"] = "CompactJSON";
	j2["null"] = nullptr;

	//insert JSON-array in key "array"
	j2["array"] = { 1,  true,  -8,  3.4,  "str" };

	//or use element index to insert element in position
	j2["array"][5] = {"subarray", 34., nullptr};
	j2["object"] = {{"value1", 1426}};
	j2["object"]["value2"] = {"everything"};

	std::ifstream in("test.json");
	JSON j3;
	in >> j3;

	//load from string (using raw string literals)
	JSON j4 = JSON::from_string(R"(
		{
    		"integer": 42,
    		"float": 2.6,
    		"bool": false,
    		"string": "CompactJSON",
    		"null": null,
			"obj": {
				"val": 23.4e5,
				"num": 21 
			}
		}
	)");

	std::cout << j4 << std::endl;
	//print {"bool":false,"float":2.6,"integer":42,"null":null,"obj":{"num":21,"val":2.34e+06},"string":"CompactJSON"}

	std::cout << j4.to_string() << std::endl;//result is same as previous string

	std::cout << j4.to_string(4) << std::endl;//4 - tab size
	/* result is:
		{
    		"bool": false,
    		"float": 2.6,
    		"integer": 42,
    		"null": null,
    		"obj": {
    		    "num": 21,
    		    "val": 2.34e+06
    		},
    		"string": "CompactJSON"
		}
	*/

	//(same as printing JSON in std::cout you can write it in file) 

	//get data from json object
	auto v1 = j4["float"].get<float>();
	auto v2 = j4["obj"]["num"].get<int>();

	std::cout << v1 << ' ' << v2 << std::endl;

	if(j4["obj"].is_object() && j4["obj"].contains("val")) {
		auto val = j4["obj"]["val"].get<float>();
		std::cout << val << std::endl;
	}

	if(j1 == j2) std::cout << "j1 & j2 same objects" << std::endl;

	j1["new_key"] = 22;

	if(j1 != j2) std::cout << "j1 & j2 different objects" << std::endl;

	
}