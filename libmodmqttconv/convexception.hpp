#pragma once

#include <exception>

class ConvException : public std::exception {
    public:
        ConvException(const char* what) : mWhat(what) {}
        virtual const char* what() const throw() { return mWhat ; }
    protected:
        const char* mWhat;
};
