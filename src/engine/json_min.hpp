#pragma once

#include <cctype>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace vitalgrid {

struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    double number = 0;
    std::string text;
    std::vector<Json> array;
    std::map<std::string, Json> object;

    const Json* find(const std::string& key) const {
        if (type != Type::Object) return nullptr;
        auto it = object.find(key);
        return it == object.end() ? nullptr : &it->second;
    }

    double number_or(double fallback) const {
        return type == Type::Number ? number : fallback;
    }
};

inline Json parse_json(const std::string& input) {
    struct Parser {
        const std::string& in;
        size_t i = 0;

        explicit Parser(const std::string& source) : in(source) {}

        void skip() {
            while (i < in.size() && std::isspace(static_cast<unsigned char>(in[i]))) i++;
        }

        char peek() {
            skip();
            if (i >= in.size()) throw std::runtime_error("JSON incompleto");
            return in[i];
        }

        char get() {
            char c = peek();
            i++;
            return c;
        }

        Json parse() {
            Json value = parse_value();
            skip();
            if (i != in.size()) throw std::runtime_error("JSON con texto de sobra");
            return value;
        }

        Json parse_value() {
            char c = peek();
            if (c == '{') return parse_object();
            if (c == '[') return parse_array();
            if (c == '"') return parse_string();
            if (c == 't' || c == 'f') return parse_bool();
            if (c == 'n') return parse_null();
            return parse_number();
        }

        Json parse_object() {
            Json value;
            value.type = Json::Type::Object;
            get();
            if (peek() == '}') {
                get();
                return value;
            }
            while (true) {
                if (peek() != '"') throw std::runtime_error("clave JSON invalida");
                std::string key = parse_string().text;
                if (get() != ':') throw std::runtime_error("falta : en JSON");
                value.object.emplace(key, parse_value());
                char next = get();
                if (next == '}') break;
                if (next != ',') throw std::runtime_error("falta coma en objeto JSON");
            }
            return value;
        }

        Json parse_array() {
            Json value;
            value.type = Json::Type::Array;
            get();
            if (peek() == ']') {
                get();
                return value;
            }
            while (true) {
                value.array.push_back(parse_value());
                char next = get();
                if (next == ']') break;
                if (next != ',') throw std::runtime_error("falta coma en arreglo JSON");
            }
            return value;
        }

        Json parse_string() {
            if (get() != '"') throw std::runtime_error("se esperaba una cadena");
            Json value;
            value.type = Json::Type::String;
            while (i < in.size()) {
                char c = in[i++];
                if (c == '"') return value;
                if (c == '\\') {
                    if (i >= in.size()) throw std::runtime_error("escape JSON incompleto");
                    char e = in[i++];
                    if (e == '"' || e == '\\' || e == '/') value.text.push_back(e);
                    else if (e == 'b') value.text.push_back('\b');
                    else if (e == 'f') value.text.push_back('\f');
                    else if (e == 'n') value.text.push_back('\n');
                    else if (e == 'r') value.text.push_back('\r');
                    else if (e == 't') value.text.push_back('\t');
                    else throw std::runtime_error("escape JSON no soportado");
                } else {
                    value.text.push_back(c);
                }
            }
            throw std::runtime_error("cadena JSON sin cerrar");
        }

        Json parse_bool() {
            Json value;
            value.type = Json::Type::Bool;
            if (in.compare(i, 4, "true") == 0) {
                value.boolean = true;
                i += 4;
                return value;
            }
            if (in.compare(i, 5, "false") == 0) {
                value.boolean = false;
                i += 5;
                return value;
            }
            throw std::runtime_error("booleano JSON invalido");
        }

        Json parse_null() {
            if (in.compare(i, 4, "null") != 0) throw std::runtime_error("null JSON invalido");
            i += 4;
            return Json{};
        }

        Json parse_number() {
            size_t start = i;
            if (peek() == '-') i++;
            if (i >= in.size() || !std::isdigit(static_cast<unsigned char>(in[i]))) {
                throw std::runtime_error("numero JSON invalido");
            }
            while (i < in.size() && std::isdigit(static_cast<unsigned char>(in[i]))) i++;
            if (i < in.size() && in[i] == '.') {
                i++;
                while (i < in.size() && std::isdigit(static_cast<unsigned char>(in[i]))) i++;
            }
            Json value;
            value.type = Json::Type::Number;
            value.number = std::stod(in.substr(start, i - start));
            return value;
        }
    };

    return Parser(input).parse();
}

} // namespace vitalgrid
