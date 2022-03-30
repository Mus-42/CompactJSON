#include <iostream>
#include <fstream>
#include "json.hpp"
using CompactJSON::JSON;
int main() {
	//implicit construct json using initializer list 

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

	//insert 42 (JSON-number) value in object whith key "integer" ...
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
	j2["object"]["value2"] = "everything";

	//load from file using >> (istream) operator
	JSON j3;
	std::ifstream in("test.json");
	if(in.is_open()) in >> j3;

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
	std::cout  << std::endl;

	//(same as printing JSON in std::cout you can write it in file) 

	//get data from json object
	auto v1 = j4["float"].get<float>();
	auto v2 = j4["obj"]["num"].get<int>();

	std::cout << v1 << ' ' << v2 << std::endl;

	if(j4["obj"].is_object() && j4["obj"].contains("val")) {
		auto val = j4["obj"]["val"].get<float>();
		std::cout << val << std::endl;
	}
	std::cout  << std::endl;

	if(j1 == j2)//true
		std::cout << "j1 & j2 same objects" << std::endl;

	j1["new_key"] = 22;//change j1

	if(j1 != j2)//true
		std::cout << "j1 & j2 different objects" << std::endl;

	if(j1 == j2)//false
		std::cout << "j1 & j2 same objects" << std::endl;

	std::cout  << std::endl;

	//clear j1
	j1.clear();
	//now j1 is null

	auto j5 = j3;//copy
	auto j6 = std::move(j3);//move

	//range based for (for arrays and objects)
	
	for(auto v : j4) std::cout << v << std::endl;
	
	std::cout  << std::endl;
	//iterators.
	//for objects iterator return only values, not key-value pairs.
	for(auto it = j4.crbegin(); it != j4.crend(); it++) std::cout << it->to_string(4) << std::endl;
	std::cout  << std::endl;
	//array
	JSON j7 = {"array:", 1, 2, 3, {"subarray", "2 3 7"}};
	for(auto v : j7) std::cout << v << std::endl;
	std::cout  << std::endl;

	//!!!!
	JSON array = {"str", 42};//is array
	JSON object = {{"str", 42}};//is object
	std::cout << array << ' ' << object << std::endl;
}