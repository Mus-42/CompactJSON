#pragma once
#ifndef JSON_H_INCLUDE_HEADER_
#define JSON_H_INCLUDE_HEADER_

#include <string>
#include <vector>
#include <map>
#include <iterator> //random_access_iterator_tag
#include <functional>
#include <algorithm>
#include <type_traits>
#include <sstream>
#include <iostream>
#include <exception>

#include <cassert>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <cstdint> //int64_t

//you can define custom assert or parse error macros and override it

#ifndef JSON_PARSE_ERROR
#define JSON_PARSE_ERROR(description) throw std::runtime_error(description);
#endif

#ifndef JSON_TYPE_ASSERT
#define JSON_TYPE_ASSERT(v) { if(!(v)) throw std::logic_error("json: invalid type"); }
#endif

#ifndef JSON_ASSERT
#ifdef NDEBUG
#define JSON_ASSERT(v) {}
#else
#define JSON_ASSERT(v) CompactJSON::detail::json_assert_impl(v, __LINE__, __FILE__)
#endif//NDEBUG
#endif//JSON_ASSERT

namespace CompactJSON {
    class JSONBase;
    namespace detail {
        inline void json_assert_impl(bool passed, int line, const std::string& file, bool fatal = true) {
            if(!passed) {
                std::cout << "assertion failed in " << file << " (l: " << line << ")" << std::endl;
                if(fatal) std::exit(1); 
            }
        }
        template <typename JSON_, typename array_it, typename object_it>
        class JSONIteratorBase {
        protected:
            enum class iter_t : uint8_t { 
                none = 0, array, object
            };
        public:
            using iterator_category = std::bidirectional_iterator_tag;
            using value_type = JSON_;
            using difference_type = ptrdiff_t;
            using pointer = value_type *;
            using reference = value_type &;
            using const_reference = std::add_const_t<value_type> &;
            explicit JSONIteratorBase() noexcept = default;
            ~JSONIteratorBase() {
                switch(m_type) {
                case iter_t::array: iter_array.~array_it(); break;
                case iter_t::object: iter_object.~object_it(); break;
                default: break;
                }
            }
            JSONIteratorBase(const JSONIteratorBase &o) { *this = o; }
            explicit JSONIteratorBase(const JSONBase *p, array_it arr_i) : m_type(iter_t::array), iter_array(arr_i), parent(p) {}
            explicit JSONIteratorBase(const JSONBase *p, object_it obj_i) : m_type(iter_t::object), iter_object(obj_i), parent(p) {}
            JSONIteratorBase &operator=(const JSONIteratorBase &o) noexcept {
                this->~JSONIteratorBase();
                switch(m_type = o.m_type) {
                case iter_t::array:
                    new (&iter_array) array_it(o.iter_array);
                    break;
                case iter_t::object:
                    new (&iter_object) object_it(o.iter_object);
                    break;
                default: break;
                }
                return *this;
            }
            reference operator*() const noexcept { return *((*this).operator->()); }
            pointer operator->() const noexcept {
                switch(m_type) {
                case iter_t::array: return *iter_array;
                case iter_t::object: return iter_object->second;
                default: return nullptr;
                }
                return nullptr;
            };
            JSONIteratorBase &operator++() noexcept {
                switch(m_type) {
                case iter_t::array: iter_array++; break;
                case iter_t::object: iter_object++; break;
                default: break;
                }
                return *this;
            }
            JSONIteratorBase operator++(int) noexcept {
                auto tmp = *this;
                ++*this;
                return tmp;
            }
            JSONIteratorBase &operator--() noexcept {
                switch(m_type) {
                case iter_t::array: iter_array--; break;
                case iter_t::object: iter_object--; break;
                default: break;
                }
                return *this;
            }
            JSONIteratorBase operator--(int) noexcept {
                auto tmp = *this;
                --*this;
                return tmp;
            }
            bool operator==(const JSONIteratorBase &o) const {
                if(m_type != o.m_type)
                    return false;
                switch(m_type) {
                case iter_t::array: return iter_array == o.iter_array;
                case iter_t::object: return iter_object == o.iter_object;
                default: break;
                }
                return true;
            }
            bool operator!=(const JSONIteratorBase &o) const { return !(*this == o); }
        protected:
            friend class JSONBase;
            union {
                array_it iter_array;
                object_it iter_object;
            }; //iter value
            const JSONBase *parent = nullptr;
            iter_t m_type = iter_t::none;
        };
    } //namespace detail
    class JSONBase {
        enum class val_t : uint8_t {
            null_t = 0, float_t, int_t, bool_t, string_t, object_t, array_t
        };
    public:
        using iterator = detail::JSONIteratorBase<JSONBase, std::vector<JSONBase *>::iterator, std::map<std::string, JSONBase *>::iterator>;
        using const_iterator = detail::JSONIteratorBase<const JSONBase, std::vector<JSONBase *>::const_iterator, std::map<std::string, JSONBase *>::const_iterator>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        JSONBase() {}
        ~JSONBase() { clear(); }
        JSONBase(const JSONBase &v) { *this = v; }     //use copy assignment operator
        JSONBase(JSONBase &&v) { *this = v; }          //use move assignment operator
        JSONBase &operator=(const JSONBase &v) {
            set_type_to(v.m_type);
            switch(m_type) { //copy value
            case val_t::float_t: d = v.d; break;
            case val_t::int_t: i = v.i; break;
            case val_t::bool_t: b = v.b; break;
            case val_t::string_t: str = v.str; break;
            case val_t::object_t:
                for(auto &[key, val] : v.obj)
                    obj[key] = new JSONBase(*val);
                break;
            case val_t::array_t:
                arr.resize(v.arr.size());
                for(size_t i = 0; i < v.arr.size(); i++)
                    arr[i] = new JSONBase(*v.arr[i]);
                break;
            case val_t::null_t: default: break;
            }
            return *this;
        }
        JSONBase &operator=(JSONBase &&v) {
            set_type_to(v.m_type);
            switch(m_type) { //move value
            case val_t::float_t: d = v.d; break;
            case val_t::int_t: i = v.i; break;
            case val_t::bool_t: b = v.b; break;
            case val_t::string_t:
                str = std::move(v.str);
                break;
            case val_t::object_t:
                obj = std::move(v.obj);
                v.obj.clear();
                break;
            case val_t::array_t:
                arr = std::move(v.arr);
                v.arr.clear();
                break;
            case val_t::null_t: default: break;
            }
            return *this;
        }
        template <typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
        JSONBase(T val) : m_type(val_t::float_t), d(static_cast<double>(val)) {} //double
        template <typename T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
        JSONBase(T val) : m_type(val_t::int_t), i(static_cast<int64_t>(val)) {} //int
        template <typename T, std::enable_if_t<std::is_same_v<T, bool>, bool> = true>
        JSONBase(T val) : m_type(val_t::bool_t), b(val) {} //bool
        template <typename T, std::enable_if_t<std::is_constructible_v<std::string, T> && !std::is_same_v<T, std::nullptr_t>, bool> = true>
        JSONBase(T val) : m_type(val_t::string_t), str(val) {} //string
        template <typename T, std::enable_if_t<std::is_same_v<T, std::nullptr_t>, bool> = true>
        JSONBase(T val) : m_type(val_t::null_t) {} //null
        JSONBase(std::initializer_list<JSONBase> val) {
            bool is_object = true;
            for(auto &v : val)
                if(!(v.is_array() && v.arr.size() == 2 && v.arr[0]->is_string())) {
                    is_object = false;
                    break;
                }
            if(is_object) {
                set_type_to(val_t::object_t);
                for(auto &v : val)
                    obj[v.arr[0]->str] = new JSONBase(*v.arr[1]);//copy
            } else {
                set_type_to(val_t::array_t);
                for(auto &v : val)
                    arr.push_back(new JSONBase(v));//copy
            } 
        }

        static JSONBase from_string(const std::string& str, bool enable_comments = false) {
            std::istringstream s(str);
            return from_stream(s, enable_comments);
        }
        static JSONBase from_stream(std::istream& istr, bool enable_comments = false) {
            JSONBase j;
            j.scan(istr, enable_comments);
            return j;
        }

        bool is_float() const noexcept { return m_type == val_t::float_t; }
        bool is_integer() const noexcept { return m_type == val_t::int_t; }
        bool is_boolean() const noexcept { return m_type == val_t::bool_t; }
        bool is_string() const noexcept { return m_type == val_t::string_t; }
        bool is_object() const noexcept { return m_type == val_t::object_t; }
        bool is_array() const noexcept { return m_type == val_t::array_t; }
        bool is_null() const noexcept { return m_type == val_t::null_t; }
        bool is_number() const noexcept { return m_type == val_t::int_t || m_type == val_t::float_t; }


        JSONBase &operator[](const std::string &key) {
            if(is_null())
                set_type_to(val_t::object_t);
            JSON_TYPE_ASSERT(is_object());
            auto f = obj.find(key);
            if(f == obj.end())
                return *(obj[key] = new JSONBase);
            else
                return *f->second;
        }
        const JSONBase &operator[](const std::string &key) const {
            JSON_TYPE_ASSERT(is_object());
            auto f = obj.find(key);
            JSON_ASSERT(f != obj.end()); //must contains element
            return *f->second;
        }
        JSONBase &operator[](size_t i) {
            if(is_null())
                set_type_to(val_t::array_t);
            JSON_TYPE_ASSERT(is_array());
            if(arr.size() <= i) resize(i + 1);
            return *arr[i];
        }
        const JSONBase &operator[](size_t i) const {
            JSON_TYPE_ASSERT(is_array());
            JSON_ASSERT(i < arr.size());
            return *arr[i];
        }

        size_t array_size() const {
            JSON_TYPE_ASSERT(is_array());
            return arr.size();
        }
        size_t object_size() const {
            JSON_TYPE_ASSERT(is_object());
            return obj.size();
        }

        bool contains(const std::string& key) const {
            JSON_TYPE_ASSERT(is_object());
            auto f = obj.find(key);
            return f != obj.end();
        }

        void erase(const std::string& key) {
            JSON_TYPE_ASSERT(is_object());
            auto f = obj.find(key);
            JSON_ASSERT(f != obj.end());
            obj.erase(f);
        }

        void resize(size_t new_size) {
            if(is_null()) set_type_to(val_t::array_t);
            JSON_TYPE_ASSERT(is_array());
            size_t old_size = arr.size();
            arr.resize(new_size, nullptr);
            for(size_t j = old_size; j < arr.size(); j++)
                arr[j] = new JSONBase;
        }

        void clear() {
            set_type_to(val_t::null_t);
        }

        bool empty() const {
            JSON_TYPE_ASSERT(is_object() || is_array());
            return is_array() ? arr.empty() : obj.empty();
        }

        template <typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
        double &get() { JSON_TYPE_ASSERT(is_float()); return d; }
        template <typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
        const double &get() const { JSON_TYPE_ASSERT(is_float()); return d; }
        template <typename T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
        int64_t &get() { JSON_TYPE_ASSERT(is_integer()); return i; }
        template <typename T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
        const int64_t &get() const { JSON_TYPE_ASSERT(is_integer()); return i; }
        template <typename T, std::enable_if_t<std::is_same_v<T, bool>, bool> = true>
        bool &get() { JSON_TYPE_ASSERT(is_boolean()); return b; }
        template <typename T, std::enable_if_t<std::is_same_v<T, bool>, bool> = true>
        const bool &get() const { JSON_TYPE_ASSERT(is_boolean()); return b; }
        template <typename T, std::enable_if_t<std::is_same_v<T, std::string>, bool> = true>
        std::string &get() { JSON_TYPE_ASSERT(is_string()); return str; }
        template <typename T, std::enable_if_t<std::is_same_v<T, std::string>, bool> = true>
        const std::string &get() const { JSON_TYPE_ASSERT(is_string()); return str; }
        template <typename T, std::enable_if_t<std::is_same_v<T, std::nullptr_t>, bool> = true>
        std::nullptr_t get() const { JSON_TYPE_ASSERT(is_null()); return nullptr; }

        iterator begin() {
            JSON_TYPE_ASSERT(is_array() || is_object());
            return is_array() ? iterator(this, arr.begin()) : iterator(this, obj.begin());
        }
        iterator end() {
            JSON_TYPE_ASSERT(is_array() || is_object());
            return is_array() ? iterator(this, arr.end()) : iterator(this, obj.end());
        }
        const_iterator begin() const {
            JSON_TYPE_ASSERT(is_array() || is_object());
            return is_array() ? const_iterator(this, arr.begin()) : const_iterator(this, obj.begin());
        }
        const_iterator end() const {
            JSON_TYPE_ASSERT(is_array() || is_object());
            return is_array() ? const_iterator(this, arr.end()) : const_iterator(this, obj.end());
        }
        const_iterator cbegin() const { return begin(); }
        const_iterator cend() const { return end(); }
        reverse_iterator rbegin() { return reverse_iterator(end()); }
        reverse_iterator rend() { return reverse_iterator(begin()); }
        const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
        const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
        const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }
        const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }

        std::string to_string(int tab_size = -1) const { 
            std::ostringstream s;
            print(s, tab_size);
            return s.str(); 
        }

    protected:
        friend std::ostream &operator<<(std::ostream &ostr, const JSONBase &j);
        friend std::istream &operator>>(std::istream &ostr, JSONBase &j);
        friend bool operator==(const JSONBase &a, const JSONBase &b);
        friend bool operator!=(const JSONBase &a, const JSONBase &b);
        #ifdef JSON_PROTECTED_DEFINITIONS//define it to add friend functions/classes or additional member data
        JSON_PROTECTED_DEFINITIONS;
        #endif//JSON_PROTECTED_DEFINITIONS

        void print(std::ostream &ostr, int tab_size = -1, size_t space_offset = 0) const {
            auto offset = std::string(space_offset, ' ');
            auto delta = tab_size < 0 ? std::string() : std::string(tab_size, ' ');
            auto newline = tab_size < 0 ? std::string() : std::string(1, '\n');
            auto space = tab_size < 0 ? std::string() : std::string(1, ' ');
            switch(m_type) {
            case val_t::string_t:
                ostr << ('"' + escape_sec_to_string(str) + '"');
                break;
            case val_t::object_t: {
                if(obj.empty()) {
                    ostr << '{' << '}';
                    break;
                }
                size_t r = obj.size() - 1;
                ostr << '{' << newline;
                for(auto [key, v] : obj) {
                    ostr << offset << delta << '"' << escape_sec_to_string(key) << '"' << ':' << space;
                    v->print(ostr, tab_size, space_offset + (tab_size > 0 ? tab_size : 0)); 
                    (r-- ? ostr << ',' << newline : ostr << newline << offset << '}');
                }
                break;
            }
            case val_t::array_t: {
                if(arr.empty()) {
                    ostr << '[' << ']';
                    break;
                }
                size_t r = arr.size() - 1;
                ostr << '[' << newline;
                for(auto v : arr) {
                    ostr << offset << delta;
                    v->print(ostr, tab_size, space_offset + (tab_size > 0 ? tab_size : 0));
                    (r-- ? ostr << ',' << newline : ostr << newline << offset << ']');
                }
                break;
            }
            case val_t::float_t: ostr << d; break;
            case val_t::int_t: ostr << i; break;
            case val_t::bool_t: ostr << (b ? "true" : "false"); break;
            case val_t::null_t: ostr << "null"; break;
            default: break;
            }
        }
        void scan(std::istream &in, bool enable_comments = false) {
            int ch;
            JSONBase ret, *cur = &ret;
            //auto is_correct_char = [](int ch) -> bool { return -1 <= ch && ch <= 255};
            auto skip_spaces = [&](){ 
                while(!in.eof() && std::isspace(ch = in.get())) continue; 
            };
            auto skip_comments = [&](){       
                if(!enable_comments) JSON_PARSE_ERROR("json: comments is not enabled");
                ch = in.get();
                if(ch == '/') {//single line
                    while(!in.eof() && (ch = in.get()) != '\n') continue;
                } else if(ch == '*') {//multiline
                    int ch2 = in.get();
                    while(!in.eof() && !((ch = in.get()) == '/' && ch2 == '*')) 
                        ch2 = ch;
                    if(in.eof()) JSON_PARSE_ERROR("json: unexpected end of file");
                    ch = in.get();
                } else JSON_PARSE_ERROR("json: unexpected character");
            };
            std::function<std::string(void)> scan_string;
            scan_string = [&]() -> std::string {
                std::string s;
                while(!in.eof() && (ch = in.get()) != '"') {
                    if(!in.eof() && ch == '\\') {
                        ch = in.get();
                        switch(ch) {
                        case 'b': s += '\b'; break;
                        case 'f': s += '\f'; break;
                        case 'n': s += '\n'; break;
                        case 'r': s += '\r'; break;
                        case 't': s += '\t'; break;
                        case '"': s += '"'; break; 
                        case '\\': s += '\\'; break;
                        case 'u': {//unicode char
                            int symbol = 0;
                            for(size_t i = 0; i < 4; i++) {//4 hex digits
                                ch = in.get();
                                symbol = symbol * 16 + (std::isdigit(ch) ? ch-'0' : (std::isupper(ch) ? ch - 'A' : ch - 'a'));
                            }
                            //now symbol is unicode codepoint for symbol
                            //TODO: add char conversion to utf-8
                            if(ch > 127) JSON_PARSE_ERROR("json: non-ascii symbol in escape sequence");
                            s += ch;
                            break;
                        }
                        default: JSON_PARSE_ERROR("json: invalid escape sequence"); break;
                        }
                    }
                    else {
                        if(ch > 127) JSON_PARSE_ERROR("json: non-ascii symbol");
                        s += ch;
                    }
                }
                if(in.eof()) 
                    JSON_PARSE_ERROR("json: unexpected end of file");
                return s;
            };
            using num_ret_t = std::pair<std::pair<double, int64_t>, bool>;//bool = is float v?
            std::function<num_ret_t(void)> scan_number; 
            scan_number = [&]() -> num_ret_t {
                bool is_positive = ch != '-', is_exp_positive = true, is_float = false;
                if((ch == '+' || ch == '-') && !in.eof())
                    ch = in.get();
                int64_t int_part = std::isdigit(ch) ? ch - '0' : 0, fract_part = 0, fract_div = 1, exp_part = 0;
                if(std::isdigit(ch))
                    while(!in.eof() && std::isdigit(ch = in.get())) int_part = int_part * 10 + ch - '0';
                if(ch == '.') {
                    is_float = true; //fractional part
                    while(!in.eof() && std::isdigit(ch = in.get())) fract_part = fract_part * 10 + ch - '0', fract_div *= 10;
                }
                if(ch == 'e' || ch == 'E') {
                    is_float = true;
                    if(!in.eof()) ch = in.get(); //exponent (all numbers with exp is float)
                    if(ch == '-') is_exp_positive = false;
                    exp_part = std::isdigit(ch) ? ch - '0' : 0;
                    while(!in.eof() && std::isdigit(ch = in.get())) exp_part = exp_part * 10 + ch - '0';
                } //now number is: (is_positive ? 1 : -1) * (int_part + fract_part/fract_div) * 10^((is_exp_positive ? 1 : -1) * exp_part)
                return is_float ? num_ret_t{
                    {(is_positive ? 1. : -1.) * (double(int_part) + double(fract_part) / double(fract_div))//float value 
                    * std::pow(10., ((is_exp_positive ? 1. : -1.) * double(exp_part))), 0}, true}//exponent
                    : num_ret_t{{0., (is_positive ? 1 : -1) * int_part}, false};//integer
            };

            std::function<JSONBase(void)> scan_value;
            scan_value = [&]() -> JSONBase {
                JSONBase ret;
                if(std::isspace(ch)) skip_spaces();
                switch(ch) {
                case '[': {//begin array
                    ret.set_type_to(val_t::array_t);
                    size_t i = 0;
                    while(ch != ']') { 
                        skip_spaces();
                        while (std::isspace(ch) || ch == '/') {
                            if(ch == '/') skip_comments();
                            if(std::isspace(ch)) skip_spaces();
                        }
                        if(ch == ']') continue;
                        ret[i++] = scan_value();
                        if(ch == ',' || ch == ']') continue;
                        if(!in.eof()) ch = in.get();
                        while (std::isspace(ch) || ch == '/') {
                            if(ch == '/') skip_comments();
                            if(std::isspace(ch)) skip_spaces();
                        }
                        if(ch != ',' && ch != ']') 
                            JSON_PARSE_ERROR("json: ',' or ']' expected");
                    }
                    if(!in.eof()) ch = in.get();
                    break;
                }
                case '{': { //begin object
                    ret.set_type_to(val_t::object_t);
                    while(ch != '}') { //begin array
                        skip_spaces();
                        while (std::isspace(ch) || ch == '/') {
                            if(ch == '/') skip_comments();
                            if(std::isspace(ch)) skip_spaces();
                        }
                        if(ch == '}') continue;
                        if(ch != '"') 
                            JSON_PARSE_ERROR("json: '\"' expected");
                        std::string key = scan_string();
                        skip_spaces();
                        while (std::isspace(ch) || ch == '/') {
                            if(ch == '/') skip_comments();
                            if(std::isspace(ch)) skip_spaces();
                        }
                        if(ch != ':') 
                            JSON_PARSE_ERROR("json: ':' expected");
                        if(in.eof()) 
                            JSON_PARSE_ERROR("json: unexpected end of file");
                        if(!in.eof()) ch = in.get();
                        ret[key] = scan_value();
                        if(std::isspace(ch)) skip_spaces();
                        if(ch == ',' || ch == '}') continue;
                        if(!in.eof()) ch = in.get();
                        while (std::isspace(ch) || ch == '/') {
                            if(std::isspace(ch)) skip_spaces();
                            if(ch == '/') skip_comments();
                        }
                        if(ch != ',' && ch != '}')
                            JSON_PARSE_ERROR("json: ',' or '}' expected");
                    }
                    if(!in.eof()) ch = in.get();
                    break;
                }
                case '"':
                    ret = scan_string();
                    break; //string
                case '0': case '1': case '2': case '3': case '4': //number (floating point or integer)
                case '5': case '6': case '7': case '8': case '9': 
                case '.': case '-': case '+': { //can begin from . (.2 same as 0.2) or from - or + (plus is non standart)
                    auto [n, is_f] = scan_number();
                    auto [f, i] = n;
                    if(is_f) {
                        if(!std::isfinite(f)) JSON_PARSE_ERROR("json: invalid constant");
                        ret = f;
                    } else
                        ret = i;
                    break;
                }
                case 'n': {//null
                    if(!in.eof() && (ch = in.get()) == 'u' && !in.eof() && (ch = in.get()) == 'l' && !in.eof() && (ch = in.get()) == 'l')
                        ret = nullptr;
                    else JSON_PARSE_ERROR("json: unexpected character");
                    break;
                }
                case 't': {//true
                    if(!in.eof() && (ch = in.get()) == 'r' && !in.eof() && (ch = in.get()) == 'u' && !in.eof() && (ch = in.get()) == 'e')
                        ret = true;
                    else JSON_PARSE_ERROR("json: unexpected character");
                    break;
                }
                case 'f': {//false
                    if(!in.eof() && (ch = in.get()) == 'a' && !in.eof() && (ch = in.get()) == 'l' && !in.eof() && (ch = in.get()) == 's' 
                        && !in.eof() && (ch = in.get()) == 'e')
                        ret = false;
                    else JSON_PARSE_ERROR("json: unexpected character");
                    break;
                }
                case '/': {//comment begin 
                    skip_comments();
                    break;
                }
                default: {//error
                    JSON_PARSE_ERROR("json: unexpected character");
                    break;
                }
                }
                return ret;
            };
            if(!in.eof()) ch = in.get();
            if(!in.eof()) *this = scan_value();
            if(!in.eof()) {
                if(!is_array() && !is_object()) ch = in.get();
                if(std::isspace(ch)) skip_spaces();
                if(!in.eof() && !std::isspace(ch)) JSON_PARSE_ERROR("json: unexpected character");
            }
        }

    private:
        union {
            double d; int64_t i = 0; bool b; std::string str; //simple types                 
            std::map<std::string, JSONBase *> obj; //object
            std::vector<JSONBase *> arr;           //array
        };
        val_t m_type = val_t::null_t;
        static std::string escape_sec_to_string(const std::string &str) {
            std::string ret;
            for(auto ch : str)
                switch(ch) {
                case '\b': ret += '\\'; ret += 'b'; break;
                case '\f': ret += '\\'; ret += 'f'; break;
                case '\n': ret += '\\'; ret += 'n'; break;
                case '\r': ret += '\\'; ret += 'r'; break;
                case '\t': ret += '\\'; ret += 't'; break;
                case '\"': ret += '\\'; ret += '"'; break;
                case '\\': ret += '\\'; ret += '\\'; break;
                default: ret += ch; break;
                }
            return ret;
        }
        val_t set_type_to(val_t t) {
            switch(m_type) { //destruct value
            case val_t::string_t:
                str.~basic_string();
                break;
            case val_t::object_t:
                for(auto &[key, v] : obj)
                    delete v;
                obj.~map();
                break;
            case val_t::array_t:
                for(auto v : arr)
                    delete v;
                arr.~vector();
                break;
            default: break;
            }
            m_type = t;
            switch(m_type) { //construct value
            case val_t::string_t: new (&str) std::string; break;
            case val_t::object_t: new (&obj) std::map<std::string, JSONBase *>; break;
            case val_t::array_t: new (&arr) std::vector<JSONBase *>; break;
            default: i = 0; break;
            }
            return t;
        }
    };
    std::ostream &operator<<(std::ostream &ostr, const JSONBase &j) { return j.print(ostr), ostr; }
    std::istream &operator>>(std::istream &istr, JSONBase &j) { return j.scan(istr), istr; }
    bool operator==(const JSONBase &a, const JSONBase &b) {
        if(a.m_type != b.m_type)
            return false;
        using val_t = JSONBase::val_t;
        switch(a.m_type) {
        case val_t::float_t: return std::abs(a.d - b.d) < 1e-10;
        case val_t::int_t: return a.i == b.i;
        case val_t::bool_t: return a.b == b.b;
        case val_t::string_t: return a.str == b.str;
        case val_t::object_t:
            if(a.obj.size() != b.obj.size())
                return false;
            for(auto ai = a.obj.begin(), bi = b.obj.begin(); ai != a.obj.end(); ai++, bi++)
                if(ai->first != bi->first || *ai->second != *bi->second)
                    return false;
            return true;
        case val_t::array_t:
            if(a.arr.size() != b.arr.size())
                return false;
            for(size_t i = 0; i < a.arr.size(); i++)
                if(*a.arr[i] != *b.arr[i])
                    return false;
            return true;
        case val_t::null_t: return true;
        default: break;
        }
        return false;
    }
    bool operator!=(const JSONBase &a, const JSONBase &b) { return !(a == b); }
    using JSON = JSONBase;
}//namespace CompactJSON

#endif//JSON_H_INCLUDE_HEADER_