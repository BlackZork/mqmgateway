#pragma once

#include <boost/version.hpp>
#include <boost/dll/import.hpp>

namespace modmqttd {

#if BOOST_VERSION >= 108800 //1.88.0
template<class T>
std::shared_ptr<T>
boost_dll_import(
    const boost::dll::fs::path& lib, 
    const char* name,
    boost::dll::load_mode::type mode = boost::dll::load_mode::default_mode) 
    {
        return boost::dll::import_symbol<T>(lib, name, mode);
    }
#else
    //https://stackoverflow.com/questions/71572186/question-on-converting-boost-shared-pointer-to-standard-shared-pointer/71575543#71575543
    template<class T>
    std::shared_ptr<T>
    as_std_shared_ptr(boost::shared_ptr<T> bp)
    {
        if (!bp) return nullptr;
        // a std shared pointer to boost shared ptr.  Yes.
        auto pq = std::make_shared<boost::shared_ptr<T>>(std::move(bp));
        // aliasing ctor.  Hide the double shared ptr.  Sneaky.
        return std::shared_ptr<T>(pq, pq.get()->get());
    }
// < 1.88.0
#if BOOST_VERSION >= 107600 // 1.76.0

template <class T>
std::shared_ptr<T>
boost_dll_import(
    const boost::dll::fs::path& lib, 
    const char* name,
    boost::dll::load_mode::type mode = boost::dll::load_mode::default_mode) 
{
    boost::shared_ptr<T> ret(boost::dll::import_symbol<T>(lib, name, mode));
    return as_std_shared_ptr(ret);
}

#else
std::shared_ptr<T>
boost_dll_import(
    const boost::dll::fs::path& lib, 
    const char* name,
    boost::dll::load_mode::type mode = boost::dll::load_mode::default_mode) 
{
    boost::shared_ptr<T> ret(boost::dll::import<T>(lib, name, mode));
    return as_std_shared_ptr(ret);
}
#endif //>= 1.76.0
#endif //>= 1.88.0

}