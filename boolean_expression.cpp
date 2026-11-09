/*
 * Minimal implementation of GEO::BooleanExpression (parser for simple
 * boolean expressions used by GEO::ExactCDT2d::classify_triangles / CSG
 * operations). This file is not vendored in V-Sekai-fire/interactor-curvenet's
 * geogram-subset (only boolean_expression.h ships there, since curvenet.elf
 * never calls ExactCDT2d::classify_triangles) but geogram/delaunay/CDT_2d.cpp
 * references the symbols at link time regardless because CDT2d and
 * ExactCDT2d live in the same translation unit. Ported from the public
 * upstream geogram source (BrunoLevy/geogram, geogram/basic/boolean_expression.cpp),
 * same license as the rest of vendor/geogram-subset (BSD-3-Clause, Inria).
 */
#include <geogram/basic/boolean_expression.h>
#include <geogram/basic/logger.h>
#include <cctype>

namespace GEO {

    BooleanExpression::BooleanExpression(const std::string& expr) :
        expr_(expr) {
        ptr_ = expr_.begin();
    }

    bool BooleanExpression::operator()(index_t x) {
        x_ = x;
        ptr_ = expr_.begin();
        return parse_or();
    }

    bool BooleanExpression::parse_or() {
        bool left = parse_and();
        while(cur_char() == '|' || cur_char() == '+' || cur_char() == '^') {
            char op = cur_char();
            next_char();
            bool right = parse_and();
            if(op == '^') {
                left = (left != right);
            } else {
                left = left || right;
            }
        }
        return left;
    }

    bool BooleanExpression::parse_and() {
        bool left = parse_factor();
        while(cur_char() == '&' || cur_char() == '*' || cur_char() == '-') {
            char op = cur_char();
            next_char();
            bool right = parse_factor();
            if(op == '-') {
                left = left && !right;
            } else {
                left = left && right;
            }
        }
        return left;
    }

    bool BooleanExpression::parse_factor() {
        if(cur_char() == '(') {
            next_char();
            bool result = parse_or();
            if(cur_char() == ')') {
                next_char();
            }
            return result;
        }
        if(cur_char() == '!') {
            next_char();
            return !parse_factor();
        }
        return parse_variable();
    }

    bool BooleanExpression::parse_variable() {
        char c = cur_char();
        index_t var_id = 0;
        bool have_var = false;
        if(c >= 'A' && c <= 'Z') {
            var_id = index_t(c - 'A');
            have_var = true;
            next_char();
        } else if(c == 'x') {
            next_char();
            std::string num;
            while(std::isdigit((unsigned char)cur_char())) {
                num += cur_char();
                next_char();
            }
            if(!num.empty()) {
                var_id = index_t(std::stoi(num));
                have_var = true;
            }
        }
        if(!have_var) {
            return false;
        }
        return ((x_ >> var_id) & 1u) != 0;
    }

    char BooleanExpression::cur_char() const {
        if(ptr_ == expr_.end()) {
            return '\0';
        }
        // skip whitespace
        std::string::const_iterator p = ptr_;
        while(p != expr_.end() && std::isspace((unsigned char)*p)) {
            ++p;
        }
        if(p == expr_.end()) {
            return '\0';
        }
        return *p;
    }

    void BooleanExpression::next_char() {
        while(ptr_ != expr_.end() && std::isspace((unsigned char)*ptr_)) {
            ++ptr_;
        }
        if(ptr_ != expr_.end()) {
            ++ptr_;
        }
    }

}
