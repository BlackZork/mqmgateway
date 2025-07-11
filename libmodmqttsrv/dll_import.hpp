#pragma once

#include <functional>

#include <boost/version.hpp>
#include <boost/dll/import.hpp>
#include <boost/bind/bind.hpp>

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
    template<typename T> void do_release(typename boost::shared_ptr<T> const&, T*) {}
    template<typename T>
    typename std::shared_ptr<T> to_std(typename boost::shared_ptr<T> const& p)
    {
        return
            std::shared_ptr<T>(
                    p.get(),
                    boost::bind(&do_release<T>, p, std::placeholders::_1));

    }    
// < 1.88.0
#if BOOST_VERSION >= 107600 // 1.76.0

template <class T>
std::shared_ptr<T>
boost_dll_import(
    const boost::dll::fs::path& lib, 
    const char* name,
    boost::dll::load_mode::type mode) 
{
    boost::shared_ptr<T> ret(boost::dll::import_symbol<T>(lib, name, mode));
    return to_std(ret);
}

#else
std::shared_ptr<T>
boost_dll_import(
    const boost::dll::fs::path& lib, 
    const char* name,
    boost::dll::load_mode::type mode) 
{
    boost::shared_ptr<T> ret(boost::dll::import<T>(lib, name, mode));
    return to_std(ret);
}
#endif //>= 1.76.0
#endif //>= 1.88.0

}