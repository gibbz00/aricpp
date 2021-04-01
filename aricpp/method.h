/*******************************************************************************
 * ARICPP - ARI interface for C++
 * Copyright (C) 2017 Daniele Pallastrelli
 *
 * This file is part of aricpp.
 * For more information, see http://github.com/daniele77/aricpp
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************/

#ifndef ARICPP_METHOD_H_
#define ARICPP_METHOD_H_

#include <string>
#include <boost/beast/http/verb.hpp>

namespace aricpp
{

enum class Method
{
    get,
    post,
    put,
    delete_
};

/// Converts the Method enum class to a string
inline std::string ToString(Method m)
{
    static const char* d[] =
    {
        "GET",
        "POST",
        "PUT",
        "DELETE"
    };
    return d[ static_cast<std::underlying_type_t<Method>>(m) ];
}

/// Converts the Method enum class to the equivalent Beast enum
inline boost::beast::http::verb ToBeast(Method m)
{
    static const boost::beast::http::verb d[] =
    {
        boost::beast::http::verb::get,
        boost::beast::http::verb::post,
        boost::beast::http::verb::put,
        boost::beast::http::verb::delete_
    };
    return d[static_cast<std::underlying_type_t<Method>>(m)];
}

} // namespace aricpp

#endif // ARICPP_METHOD_H_
