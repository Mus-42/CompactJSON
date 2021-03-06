#pragma once
#ifndef JSON_H_INCLUDE_HEADER_
#define JSON_H_INCLUDE_HEADER_

#include <string>
#include <vector>
#include <array>
#include <map>
#include <iterator> //random_access_iterator_tag
#include <functional>
#include <algorithm>
#include <type_traits>
#include <variant>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <exception>
#include <numeric>

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
#define JSON_ASSERT(v) CompactJSON::details::json_assert_impl(v, __LINE__, __FILE__)
#endif//NDEBUG
#endif//JSON_ASSERT

namespace CompactJSON {
    class JSONBase;
    namespace details {
        inline void json_assert_impl(bool passed, int line, const std::string& file, bool fatal = true) {
            if (!passed) {
                std::cout << "assertion failed in " << file << " (l: " << line << ")" << std::endl;
                if (fatal)
                    //std::exit(1);
                    throw std::runtime_error("assertion failed");
            }
        }
        //TODO move here scan_string (rename to string from escape ...?)
        inline std::string escape_sec_to_string(const std::string& str) {//str is unicode (utf8) string
            std::string ret;
            for (size_t i = 0; i < str.size(); i++) {
                unsigned char ch = static_cast<unsigned char>(str[i]);
                switch (ch) {
                case '\b': ret += '\\'; ret += 'b'; break;
                case '\f': ret += '\\'; ret += 'f'; break;
                case '\n': ret += '\\'; ret += 'n'; break;
                case '\r': ret += '\\'; ret += 'r'; break;
                case '\t': ret += '\\'; ret += 't'; break;
                case '\"': ret += '\\'; ret += '"'; break;
                case '\\': ret += '\\'; ret += '\\'; break;
                default: {
                    if (std::isprint(ch) && ch < 128) ret += ch;//printable ascii character
                    else {//non printable or unicode. convert to Unicode
                        ret += '\\'; ret += 'u';// "\u"
                        uint32_t char_code = 0;
                        if (ch < 0b11000000) char_code = ch;//1 wide
                        else if (ch < 0b11100000 && i + 1 < str.size())//2 wide
                            char_code = ((uint32_t(static_cast<unsigned char>(ch)) & 0b00011111) << 6) | ((uint32_t(static_cast<unsigned char>(str[i + 1])) & 0b00111111));
                        else if (ch < 0b11110000 && i + 2 < str.size())//3 wide
                            char_code = ((((uint32_t(static_cast<unsigned char>(ch)) & 0b00001111) << 6) | (uint32_t(static_cast<unsigned char>(str[i + 1])) & 0b00111111)) << 6)
                            | (uint32_t(static_cast<unsigned char>(str[i + 2])) & 0b00111111);
                        else if (i + 3 < str.size()) {//4 wide
                            char_code = ((((((uint32_t(static_cast<unsigned char>(ch)) & 0b00000111) << 6) | (uint32_t(static_cast<unsigned char>(str[i + 1])) & 0b00111111)) << 6)
                                          | (uint32_t(static_cast<unsigned char>(str[i + 2])) & 0b00111111)) << 6) | uint32_t(static_cast<unsigned char>(str[i + 3])) & 0b00111111;
                        }
                        if(char_code > (1 << 16)) {
                            JSON_ASSERT(0);
                        }
                        char buf[4];
                        for (size_t i = 0; i < 4; i++) {
                            int v = char_code % 16;
                            buf[3 - i] = v < 10 ? '0' + v : 'A' + v - 10;
                            char_code /= 16;
                        }
                        JSON_ASSERT(char_code == 0);//converted successful
                        for (size_t i = 0; i < 4; i++) ret += buf[i];
                    }
                } break;
                }
            }
            return ret;
        }

        //TODO move scan string here?
        inline std::variant<int64_t, double> scan_number(int& ch, std::istream& in) {
            bool is_positive = ch != '-', has_float = false, has_exp = false;
            if ((ch == '+' || ch == '-') && in.good()) ch = in.get();
            std::string int_part, fract_part;
            int exp_part = 0;
            if (std::isdigit(ch)) {
                int_part += ch;
                while (in.good() && std::isdigit(ch = in.get())) int_part += ch;
            }
            if (ch == '.') {
                has_float = true; //fractional part
                while (in.good() && std::isdigit(ch = in.get())) fract_part += ch;
                if(fract_part.size() > 308) fract_part.resize(308);
            }
            if (ch == 'e' || ch == 'E') {
                has_exp = true;
                bool is_exp_positive = true;
                ch = in.get(); //exponent (all numbers with exp is float)
                if (ch == '-') is_exp_positive = false;
                size_t exp_size = 0;
                if (std::isdigit(ch)) exp_part += ch - '0', exp_size = 1;
                int prev_exp = 0;
                while (in.good() && std::isdigit(ch = in.get())) {
                    prev_exp = exp_part;
                    exp_part = exp_part * 10 + ch - '0';//TODO overflow check
                    exp_size++;
                    if(prev_exp > exp_part) JSON_PARSE_ERROR("json: number exponent overflow");
                }

                if(exp_size == 0) JSON_PARSE_ERROR("json: number invalid exponent");
                if(!is_exp_positive) exp_part = -exp_part;
                if(exp_part > 308) JSON_PARSE_ERROR("json: number too large exponent");//TODO return +inf?
                if(exp_part < -308) return 0.;
            } //now number is: (is_positive ? 1 : -1) * (int_part + fract_part/fract_div) * 10^((is_exp_positive ? 1 : -1) * exp_part)

            static std::array<double, 308*2+1> pow10_table = ([](){
                static std::array<double, 308*2+1> ret;
                for(size_t i = 0; i <= 308*2; i++) ret[i] = std::pow(10, int(i)-308);
                return ret;
            })();

            if(has_exp || has_float) {
                double iv = int_part.size() ? std::stod(int_part) : 0.;
                double fv = fract_part.size() ? std::stod(fract_part) * pow10_table[308 - fract_part.size()] : 0.;
                return std::copysign((iv + fv) * pow10_table[308 + exp_part], is_positive ? 1. : -1.);
            }

            if(int_part.size() == 0) JSON_PARSE_ERROR("json: invalid number");
            
            bool has_overflow = int_part.size() > 19;

            int64_t int_val = 0, prev_val = 0;
            if(!has_overflow) for(auto ch : int_part) {
                prev_val = int_val;
                int_val *= 10, int_val += ch - '0';
                if(prev_val > int_val) {
                    has_overflow = true;  
                    break;
                }
            }

            if(has_overflow) return std::copysign(std::stod(int_part), is_positive ? 1. : -1.);
            return (is_positive ? int_val : -int_val);
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
            using pointer = value_type*;
            using reference = value_type&;
            using const_reference = std::add_const_t<value_type>&;
            explicit JSONIteratorBase() noexcept = default;
            ~JSONIteratorBase() {
                switch (m_type) {
                case iter_t::array: iter_array.~array_it(); break;
                case iter_t::object: iter_object.~object_it(); break;
                default: break;
                }
            }
            JSONIteratorBase(const JSONIteratorBase& o) { *this = o; }
            explicit JSONIteratorBase(const JSONBase* p, array_it arr_i) : m_type(iter_t::array), iter_array(arr_i), parent(p) {}
            explicit JSONIteratorBase(const JSONBase* p, object_it obj_i) : m_type(iter_t::object), iter_object(obj_i), parent(p) {}
            JSONIteratorBase& operator=(const JSONIteratorBase& o) noexcept {
                this->~JSONIteratorBase();
                switch (m_type = o.m_type) {
                case iter_t::array:
                    new (&iter_array) array_it(o.iter_array); break;
                case iter_t::object:
                    new (&iter_object) object_it(o.iter_object); break;
                default: break;
                }
                return *this;
            }
            reference operator*() const noexcept { return *((*this).operator->()); }
            pointer operator->() const noexcept {
                switch (m_type) {
                case iter_t::array: return *iter_array;
                case iter_t::object: return iter_object->second;
                default: return nullptr;
                }
                return nullptr;
            };
            JSONIteratorBase& operator++() noexcept {
                switch (m_type) {
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
            JSONIteratorBase& operator--() noexcept {
                switch (m_type) {
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
            bool operator==(const JSONIteratorBase& o) const {
                if (m_type != o.m_type) return false;
                switch (m_type) {
                case iter_t::array: return iter_array == o.iter_array;
                case iter_t::object: return iter_object == o.iter_object;
                default: break;
                }
                return true;
            }
            bool operator!=(const JSONIteratorBase& o) const { return !(*this == o); }
        protected:
            friend class JSONBase;
            union {
                array_it iter_array;
                object_it iter_object;
            }; //iter value
            const JSONBase* parent = nullptr;
            iter_t m_type = iter_t::none;
        };
    } //namespace details
    class JSONBase {
        enum class val_t : uint8_t {
            null_t = 0, float_t, int_t, bool_t, string_t, object_t, array_t
        };
    public:
        using iterator = details::JSONIteratorBase<JSONBase, std::vector<JSONBase*>::iterator, std::map<std::string, JSONBase*>::iterator>;
        using const_iterator = details::JSONIteratorBase<const JSONBase, std::vector<JSONBase*>::const_iterator, std::map<std::string, JSONBase*>::const_iterator>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        JSONBase() {}
        ~JSONBase() { clear(); }
        JSONBase(const JSONBase& v) { *this = v; }     //use copy assignment operator
        JSONBase(JSONBase&& v) { *this = v; }          //use move assignment operator
        JSONBase& operator=(const JSONBase& v) {
            set_type_to(v.m_type);
            switch (m_type) { //copy value
            case val_t::float_t: d = v.d; break;
            case val_t::int_t: i = v.i; break;
            case val_t::bool_t: b = v.b; break;
            case val_t::string_t: str = v.str; break;
            case val_t::object_t:
                for (auto& [key, val] : v.obj)
                    obj[key] = new JSONBase(*val);
                break;
            case val_t::array_t:
                arr.resize(v.arr.size());
                for (size_t i = 0; i < v.arr.size(); i++)
                    arr[i] = new JSONBase(*v.arr[i]);
                break;
            case val_t::null_t: default: break;
            }
            return *this;
        }
        JSONBase& operator=(JSONBase&& v) {
            set_type_to(v.m_type);
            switch (m_type) { //move value
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
            for (auto& v : val)
                if (!(v.is_array() && v.arr.size() == 2 && v.arr[0]->is_string())) {
                    is_object = false;
                    break;
                }
            if (is_object) {
                set_type_to(val_t::object_t);
                for (auto& v : val)
                    obj[v.arr[0]->str] = new JSONBase(*v.arr[1]);//copy
            }
            else {
                set_type_to(val_t::array_t);
                for (auto& v : val)
                    arr.push_back(new JSONBase(v));//copy
            }
        }

        [[nodiscard]] static JSONBase from_string(const std::string& str, bool enable_comments = false) {
            std::istringstream s(str);
            return from_stream(s, enable_comments);
        }
        [[nodiscard]] static JSONBase from_stream(std::istream& istr, bool enable_comments = false) {
            JSONBase j;
            j.scan(istr, enable_comments);
            return j;
        }
        //TODO add from_file (with BOM check)

        [[nodiscard]] bool is_float() const noexcept { return m_type == val_t::float_t; }
        [[nodiscard]] bool is_integer() const noexcept { return m_type == val_t::int_t; }
        [[nodiscard]] bool is_boolean() const noexcept { return m_type == val_t::bool_t; }
        [[nodiscard]] bool is_string() const noexcept { return m_type == val_t::string_t; }
        [[nodiscard]] bool is_object() const noexcept { return m_type == val_t::object_t; }
        [[nodiscard]] bool is_array() const noexcept { return m_type == val_t::array_t; }
        [[nodiscard]] bool is_null() const noexcept { return m_type == val_t::null_t; }
        [[nodiscard]] bool is_number() const noexcept { return m_type == val_t::int_t || m_type == val_t::float_t; }


        [[nodiscard]] JSONBase& operator[](const std::string& key) {
            if (is_null())
                set_type_to(val_t::object_t);
            JSON_TYPE_ASSERT(is_object());
            auto f = obj.find(key);
            if (f == obj.end())
                return *(obj[key] = new JSONBase);
            else
                return *f->second;
        }
        [[nodiscard]] const JSONBase& operator[](const std::string& key) const {
            JSON_TYPE_ASSERT(is_object());
            auto f = obj.find(key);
            JSON_ASSERT(f != obj.end()); //must contains element
            return *f->second;
        }
        [[nodiscard]] JSONBase& operator[](size_t i) {
            if (is_null())
                set_type_to(val_t::array_t);
            JSON_TYPE_ASSERT(is_array());
            if (arr.size() <= i) resize(i + 1);
            return *arr[i];
        }
        [[nodiscard]] const JSONBase& operator[](size_t i) const {
            JSON_TYPE_ASSERT(is_array());
            JSON_ASSERT(i < arr.size());
            return *arr[i];
        }

        [[nodiscard]] size_t array_size() const {
            JSON_TYPE_ASSERT(is_array());
            return arr.size();
        }
        [[nodiscard]] size_t object_size() const {
            JSON_TYPE_ASSERT(is_object());
            return obj.size();
        }

        [[nodiscard]] bool contains(const std::string& key) const {
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
            if (is_null()) set_type_to(val_t::array_t);
            JSON_TYPE_ASSERT(is_array());
            size_t old_size = arr.size();
            arr.resize(new_size, nullptr);
            for (size_t j = old_size; j < arr.size(); j++)
                arr[j] = new JSONBase;
        }

        void clear() {
            set_type_to(val_t::null_t);
        }

        [[nodiscard]] bool empty() const {
            JSON_TYPE_ASSERT(is_object() || is_array());
            return is_array() ? arr.empty() : obj.empty();
        }

        template <typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
        [[nodiscard]] double& get() { JSON_TYPE_ASSERT(is_float()); return d; }
        template <typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
        [[nodiscard]] const double& get() const { JSON_TYPE_ASSERT(is_float()); return d; }
        template <typename T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
        [[nodiscard]] int64_t& get() { JSON_TYPE_ASSERT(is_integer()); return i; }
        template <typename T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
        [[nodiscard]] const int64_t& get() const { JSON_TYPE_ASSERT(is_integer()); return i; }
        template <typename T, std::enable_if_t<std::is_same_v<T, bool>, bool> = true>
        [[nodiscard]] bool& get() { JSON_TYPE_ASSERT(is_boolean()); return b; }
        template <typename T, std::enable_if_t<std::is_same_v<T, bool>, bool> = true>
        [[nodiscard]] const bool& get() const { JSON_TYPE_ASSERT(is_boolean()); return b; }
        template <typename T, std::enable_if_t<std::is_same_v<T, std::string>, bool> = true>
        [[nodiscard]] std::string& get() { JSON_TYPE_ASSERT(is_string()); return str; }
        template <typename T, std::enable_if_t<std::is_same_v<T, std::string>, bool> = true>
        [[nodiscard]] const std::string& get() const { JSON_TYPE_ASSERT(is_string()); return str; }
        template <typename T, std::enable_if_t<std::is_same_v<T, std::nullptr_t>, bool> = true>
        [[nodiscard]] std::nullptr_t get() const { JSON_TYPE_ASSERT(is_null()); return nullptr; }

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

        [[nodiscard]] std::string to_string(int tab_size = -1) const {
            std::ostringstream s;
            print(s, tab_size);
            return s.str();
        }

    protected:
        friend std::ostream& operator<<(std::ostream& ostr, const JSONBase& j);
        friend std::istream& operator>>(std::istream& ostr, JSONBase& j);
        friend bool operator==(const JSONBase& a, const JSONBase& b);
        friend bool operator!=(const JSONBase& a, const JSONBase& b);
#ifdef JSON_PROTECTED_DEFINITIONS//define it to add friend functions/classes or additional member data
        JSON_PROTECTED_DEFINITIONS;
#endif//JSON_PROTECTED_DEFINITIONS

        void print(std::ostream& ostr, int tab_size = -1, size_t space_offset = 0) const {
            auto offset = std::string(space_offset, ' ');
            auto delta = tab_size < 0 ? std::string() : std::string(tab_size, ' ');
            auto newline = tab_size < 0 ? std::string() : std::string(1, '\n');
            auto space = tab_size < 0 ? std::string() : std::string(1, ' ');
            switch (m_type) {
            case val_t::string_t:
                ostr << ('"' + details::escape_sec_to_string(str) + '"');
            break;
            case val_t::object_t: {
                if (obj.empty()) {
                    ostr << '{' << '}';
                    break;
                }
                size_t r = obj.size() - 1;
                ostr << '{' << newline;
                for (auto [key, v] : obj) {
                    ostr << offset << delta << '"' << details::escape_sec_to_string(key) << '"' << ':' << space;
                    v->print(ostr, tab_size, space_offset + (tab_size > 0 ? tab_size : 0));
                    (r-- ? ostr << ',' << newline : ostr << newline << offset << '}');
                }
                break;
            }
            case val_t::array_t: {
                if (arr.empty()) {
                    ostr << '[' << ']';
                    break;
                }
                size_t r = arr.size() - 1;
                ostr << '[' << newline;
                for (auto v : arr) {
                    ostr << offset << delta;
                    v->print(ostr, tab_size, space_offset + (tab_size > 0 ? tab_size : 0));
                    (r-- ? ostr << ',' << newline : ostr << newline << offset << ']');
                }
                break;
            }
            case val_t::float_t: {
                if (!std::isfinite(d)) {
                    ostr << "null";
                    break;
                }
                if (std::abs(std::log10(d)) > 16) ostr << std::scientific;
                else ostr << std::defaultfloat << std::setprecision((std::numeric_limits<double>::max_digits10 + 1));
                //ostr << std::scientific << std::setprecision((std::numeric_limits<double>::max_digits10 + 1));
                ostr << d;//TODO fix loat precision
            } break;
            case val_t::int_t: ostr << i; break;
            case val_t::bool_t: ostr << (b ? "true" : "false"); break;
            case val_t::null_t: ostr << "null"; break;
            default: break;
            }
        }
        void scan(std::istream& in, bool enable_comments = false) {
            int ch;
            JSONBase ret, * cur = &ret;
            //auto is_correct_char = [](int ch) -> bool { return -1 <= ch && ch <= 255};
            int ch2 = 0;
            auto skip_spaces_and_comments = [&]() {
                while (in.good() && (std::isspace(ch) || ch == '/')) {
                    if (ch == '/') {
                        if (!enable_comments) JSON_PARSE_ERROR("json: comments is not enabled");
                        ch2 = in.get();
                        if (ch2 == '/') {
                            while (in.good() && ch != '\n') ch = in.get();
                            ch = in.get();
                        }
                        else if (ch2 == '*') {
                            ch2 = in.get();
                            ch = in.get();
                            while (in.good() && !(ch2 == '*' && ch == '/')) {
                                ch2 = ch;
                                ch = in.get();
                            }
                            if (!in.good()) JSON_PARSE_ERROR("json: unexpected end of file");
                            ch = in.get();
                        }
                        else JSON_PARSE_ERROR("json: unexpected character");
                    }
                    else ch = in.get();
                }
            };
            std::function<std::string(void)> scan_string;
            scan_string = [&]() -> std::string {
                std::string s;
                while (in.good() && (ch = in.get()) != '"') {
                    if (in.good() && ch == '\\') {
                        ch = in.get();
                        switch (ch) {
                        case 'b': s += '\b'; break;
                        case 'f': s += '\f'; break;
                        case 'n': s += '\n'; break;
                        case 'r': s += '\r'; break;
                        case 't': s += '\t'; break;
                        case '"': s += '"'; break;
                        case '\\': s += '\\'; break;
                        case 'u': {//unicode char escape sec
                            uint32_t symbol = 0;
                            for (size_t i = 0; i < 4; i++) {//4 hex digits
                                ch = in.get();
                                symbol = symbol * 16 + (std::isdigit(ch) ? ch - '0' : ch + 10 - (std::isupper(ch) ? 'A' : 'a'));
                            }
                            //now symbol is unicode codepoint for symbol
                            if (symbol < (2 << 7)) s += symbol;//1 wide
                            else if (symbol < (2 << 11)) {//2 wide
                                s += 0b11000000 | ((symbol & ~0b00111111) >> 6);
                                s += 0b11000000 | (symbol & 0b00111111);
                            } 
                            else if (symbol < (2 << 16)) {//3 wide
                                uint32_t v[3];
                                v[2] = symbol & 0b00111111, symbol >>= 6;
                                v[1] = symbol & 0b00111111, symbol >>= 6;
                                v[0] = symbol;
                                s += 0b11100000 | v[0], s += 0b10000000 | v[1], s += 0b10000000 | v[2];
                            }
                            else {//4 wide
                                uint32_t v[4];
                                v[3] = symbol & 0b00111111, symbol >>= 6;
                                v[2] = symbol & 0b00111111, symbol >>= 6;
                                v[1] = symbol & 0b00111111, symbol >>= 6;
                                v[0] = symbol;
                                s += 0b11110000 | v[0], s += 0b10000000 | v[1], s += 0b10000000 | v[2], s += 0b10000000 | v[3];
                            }
                            break;
                        }
                        default: JSON_PARSE_ERROR("json: invalid escape sequence"); break;
                        }
                    }
                    else {
                        //if (ch > 127) JSON_PARSE_ERROR("json: non-ascii symbol");
                        
                        //scan string as utf-8
                        if (ch < 0b11000000) {//1 wide
                            s += ch;
                        }
                        else if (ch < 0b11100000) {//2 wide
                            if(!((ch & 0b11100000) == 0b11000000)) JSON_PARSE_ERROR("json: invalid utf8");
                            s += ch;
                            ch = in.get();
                            if(!in.good() || ((ch & 0b11000000) != 0b10000000)) JSON_PARSE_ERROR("json: invalid utf8");
                            s += ch;
                        }
                        else if (ch < 0b11110000) {//3 wide
                            if(!((ch & 0b11110000) == 0b11100000)) JSON_PARSE_ERROR("json: invalid utf8");
                            s += ch;
                            ch = in.get();
                            if(!in.good() || ((ch & 0b11000000) != 0b10000000)) JSON_PARSE_ERROR("json: invalid utf8");
                            s += ch;
                            ch = in.get();
                            if(!in.good() || ((ch & 0b11000000) != 0b10000000)) JSON_PARSE_ERROR("json: invalid utf8");
                            s += ch;
                        }
                        else {//4 wide
                            if(!((ch & 0b11111000) == 0b11110000)) JSON_PARSE_ERROR("json: invalid utf8");
                            s += ch;
                            ch = in.get();
                            if(!in.good() || ((ch & 0b11000000) != 0b10000000)) JSON_PARSE_ERROR("json: invalid utf8");
                            s += ch;
                            ch = in.get();
                            if(!in.good() || ((ch & 0b11000000) != 0b10000000)) JSON_PARSE_ERROR("json: invalid utf8");
                            s += ch;
                            ch = in.get();
                            if(!in.good() || ((ch & 0b11000000) != 0b10000000)) JSON_PARSE_ERROR("json: invalid utf8");
                            s += ch;
                        }
                    }
                }
                if (!in.good())
                    JSON_PARSE_ERROR("json: unexpected end of file");
                ch = in.get();
                return s;
            };
        
            std::function<JSONBase(void)> scan_value;
            scan_value = [&]() -> JSONBase {
                JSONBase ret;
                skip_spaces_and_comments();
                switch (ch) {
                case '[': {//begin array
                    ret.set_type_to(val_t::array_t);
                    size_t i = 0;
                    while (ch != ']') {
                        ch = in.get();
                        skip_spaces_and_comments();
                        if (ch == ']') continue;
                        ret[i++] = scan_value();
                        skip_spaces_and_comments();
                        if (ch != ',' && ch != ']')
                            JSON_PARSE_ERROR("json: ',' or ']' expected");
                    }
                    if (in.good()) ch = in.get();
                    break;
                }
                case '{': { //begin object
                    ret.set_type_to(val_t::object_t);
                    while (ch != '}') { //begin array
                        ch = in.get();
                        skip_spaces_and_comments();
                        if (ch == '}') continue;
                        if (ch != '"')
                            JSON_PARSE_ERROR("json: '\"' expected");
                        std::string key = scan_string();
                        skip_spaces_and_comments();
                        if (ch != ':')
                            JSON_PARSE_ERROR("json: ':' expected");
                        if (!in.good())
                            JSON_PARSE_ERROR("json: unexpected end of file");
                        ch = in.get();
                        ret[key] = scan_value();
                        skip_spaces_and_comments();
                        if (ch != ',' && ch != '}')
                            JSON_PARSE_ERROR("json: ',' or '}' expected");
                    }
                    if (in.good()) ch = in.get();
                    break;
                }
                case '"':
                ret = scan_string();
                break; //string
                case '0': case '1': case '2': case '3': case '4': //number (floating point or integer)
                case '5': case '6': case '7': case '8': case '9':
                case '.': case '-': case '+': { //can begin from . (.2 same as 0.2) or from - or + (plus is non standart)
                    //[int][.][fract][e[+|-]exp][literal]
                    auto n = details::scan_number(ch, in);
                    if(std::holds_alternative<int64_t>(n)) ret = std::get<int64_t>(n);
                    else ret = std::get<double>(n);
                    break;
                }
                case 'n': {//null
                    if (in.good() && (ch = in.get()) == 'u' && in.good() && (ch = in.get()) == 'l' && in.good() && (ch = in.get()) == 'l')
                        ret = nullptr;
                    else JSON_PARSE_ERROR("json: unexpected character");
                    ch = in.get();
                    break;
                }
                case 't': {//true
                    if (in.good() && (ch = in.get()) == 'r' && in.good() && (ch = in.get()) == 'u' && in.good() && (ch = in.get()) == 'e')
                        ret = true;
                    else JSON_PARSE_ERROR("json: unexpected character");
                    ch = in.get();
                    break;
                }
                case 'f': {//false
                    if (in.good() && (ch = in.get()) == 'a' && in.good() && (ch = in.get()) == 'l' && in.good() && (ch = in.get()) == 's'
                        && in.good() && (ch = in.get()) == 'e')
                        ret = false;
                    else JSON_PARSE_ERROR("json: unexpected character");
                    ch = in.get();
                    break;
                }
                default: {//error
                    JSON_PARSE_ERROR("json: unexpected character");
                    break;
                }
                }
                return ret;
            };
            if (in.good()) ch = in.get();
            if (in.good()) *this = scan_value();
            if (in.good()) {
                if (!is_array() && !is_object()) ch = in.get();
                skip_spaces_and_comments();
                if (in.good() && !std::isspace(ch)) JSON_PARSE_ERROR("json: unexpected character");
            }
        }

    private:
        union {
            double d; int64_t i = 0; bool b; std::string str; //simple types                 
            std::map<std::string, JSONBase*> obj; //object
            std::vector<JSONBase*> arr;           //array
        };
        val_t m_type = val_t::null_t;
        val_t set_type_to(val_t t) {
            switch (m_type) { //destruct value
            case val_t::string_t:
                str.~basic_string();
                break;
            case val_t::object_t:
                for (auto& [key, v] : obj)
                    delete v;
                obj.~map();
                break;
            case val_t::array_t:
                for (auto v : arr)
                    delete v;
                arr.~vector();
                break;
            default: break;
            }
            m_type = t;
            switch (m_type) { //construct value
            case val_t::string_t: new (&str) std::string; break;
            case val_t::object_t: new (&obj) std::map<std::string, JSONBase*>; break;
            case val_t::array_t: new (&arr) std::vector<JSONBase*>; break;
            default: i = 0; break;
            }
            return t;
        }
    };
    std::ostream& operator<<(std::ostream& ostr, const JSONBase& j) { return j.print(ostr), ostr; }
    std::istream& operator>>(std::istream& istr, JSONBase& j) { return j.scan(istr), istr; }
    bool operator==(const JSONBase& a, const JSONBase& b) {
        if (a.m_type != b.m_type)
            return false;
        using val_t = JSONBase::val_t;
        switch (a.m_type) {
        case val_t::float_t: return std::abs(a.d - b.d) < 1e-10;
        case val_t::int_t: return a.i == b.i;
        case val_t::bool_t: return a.b == b.b;
        case val_t::string_t: return a.str == b.str;
        case val_t::object_t:
            if (a.obj.size() != b.obj.size())
                return false;
            for (auto ai = a.obj.begin(), bi = b.obj.begin(); ai != a.obj.end(); ai++, bi++)
                if (ai->first != bi->first || *ai->second != *bi->second)
                    return false;
            return true;
        case val_t::array_t:
            if (a.arr.size() != b.arr.size())
                return false;
            for (size_t i = 0; i < a.arr.size(); i++)
                if (*a.arr[i] != *b.arr[i])
                    return false;
            return true;
        case val_t::null_t: return true;
        default: break;
        }
        return false;
    }
    bool operator!=(const JSONBase& a, const JSONBase& b) { return !(a == b); }
    using JSON = JSONBase;
}//namespace CompactJSON

#endif//JSON_H_INCLUDE_HEADER_