#pragma once

#include <exception>

class ConvException : public std::exception {
    public:
        ConvException(const char* what) : mWhat(what) {}
        ConvException(const std::string& what) : mWhat(what) {}
        virtual const char* what() const throw() { return mWhat.c_str() ; }
    protected:
        const std::string mWhat;
};
