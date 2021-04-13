/*******************************************************************************
 * ARICPP - ARI interface for C++
 * Copyright (C) 2017-2021 Daniele Pallastrelli
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

#ifndef ARICPP_HTTPCLIENT_H_
#define ARICPP_HTTPCLIENT_H_

#include <queue>
#include <string>
#include <utility>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include "detail/boostasiolib.h"
#include "basicauth.h"
#include "method.h"

// #define ARICPP_TRACE_HTTP
// #define ARICPP_HTTP_TIMEOUT

#ifdef ARICPP_HTTP_TIMEOUT
#include <boost/asio/steady_timer.hpp>
#endif

#ifdef ARICPP_TRACE_HTTP
#include <iostream>
#endif

namespace aricpp
{

class HttpClient
{
public:
    using ResponseHandler = 
        std::function<void(const boost::system::error_code&, unsigned int status, const std::string& reason, const std::string& body)>;

    HttpClient(detail::BoostAsioLib::ContextType& _ios, std::string _host, std::string _port, const std::string& user, const std::string& password) :
        ios(_ios), host(std::move(_host)), port(std::move(_port)),
        auth(GetBasicAuth(user, password)),
        resolver(ios),
        socket(ios)
#ifdef ARICPP_HTTP_TIMEOUT
        ,timer(ios)
#endif
    {}

    HttpClient() = delete;
    HttpClient(const HttpClient&) = delete;
    HttpClient(HttpClient&&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient& operator=(HttpClient&&) = delete;

    void SendRequest(Method _method, std::string _url, ResponseHandler res, std::string body = {})
    {
        // enqueue the new request
        pending.emplace(_method, std::move(_url), std::move(res), std::move(body));
        // if we're already handling another request, this one will be managed after
        // the others, so no need to connect (we're already connected)
        if (pending.size() > 1) return;

        // TODO: resolve only once inside the ctor?
        resolver.async_resolve(
            host,
            port,
            [this](const boost::system::error_code& e, boost::asio::ip::tcp::resolver::results_type i)
            {
                if (e)
                    CallBack(e);
                else
                    Connect(i);
            });
    }

private:
    struct Request
    {
        Request(Method _method, std::string _url, ResponseHandler h, std::string _body = {})
            : method(_method), url(std::move(_url)), body(std::move(_body)), onResponse(std::move(h))
        {
        }

        const Method method;
        const std::string url;
        const std::string body;
        const ResponseHandler onResponse;
    };

    void CallBack(
        const boost::system::error_code& e, boost::beast::http::status status = {}, const std::string& reason = {},
        const std::string& body = {})
    {
        assert(!pending.empty());

        pending.front().onResponse(e, static_cast<std::underlying_type_t<boost::beast::http::status>>(status), reason, body);
        pending.pop();

        if (pending.empty())
        {
            if (socket.is_open())
            {
                socket.cancel();
                socket.close();
            }
        }
        else
        {
            Write();
        }
    }

    void Connect(boost::asio::ip::tcp::resolver::results_type i)
    {
        boost::asio::async_connect(
            socket,
            std::move(i),
            [this](const boost::system::error_code& e, const boost::asio::ip::tcp::endpoint&)
            {
                if (e)
                    CallBack(e);
                else
                    Write();
            });
    }

    void Write()
    {
        assert(!pending.empty());
        auto method = pending.front().method;
        auto url = pending.front().url;
        auto body = pending.front().body;

#ifdef ARICPP_TRACE_HTTP
        std::cout << "### => " << ToString(method) << " " << url << '\n';
#endif

        // clear the request (beast does not provide a method to clear a request?!)
        boost::beast::http::request<boost::beast::http::string_body>{}.swap(request);

        request.version(11);
        request.method(ToBeast(method));
        request.target(url);
        request.set(boost::beast::http::field::host, host + ":" + port);
        request.set(boost::beast::http::field::authorization, auth);
        request.set(boost::beast::http::field::user_agent, "aricpp");
        request.set(boost::beast::http::field::content_type, "application/json");
        request.body() = body;
        request.prepare_payload();
        boost::beast::http::async_write(
            socket,
            request,
            [this](boost::system::error_code e, std::size_t bytes_transferred)
            {
                boost::ignore_unused(bytes_transferred);
                if (e)
                    CallBack(e);
                else
                    ReadResponse(); // no error, go on with read
            });
    }

    void ReadResponse()
    {
#ifdef ARICPP_HTTP_TIMEOUT
        using namespace std::chrono_literals;
        timer.expires_after(500ms);
        timer.async_wait(
            [](boost::system::error_code e)
            {
                if (e) return;
                std::cerr << "No answer received!\n";
                assert(false);
            });
#endif
        boost::beast::http::async_read(
            socket,
            buffer,
            resp,
            [this](boost::system::error_code e, std::size_t bytes_transferred)
            {
                boost::ignore_unused(bytes_transferred);
#ifdef ARICPP_HTTP_TIMEOUT
                timer.cancel();
#endif
                if (e)
                    CallBack(e);
                else
                {
#ifdef ARICPP_TRACE_HTTP
                    std::cout << "### <= " << resp.result() << " " << resp.reason() << '\n';
                    if (!resp.body().empty()) std::cout << "       " << resp.body() << '\n';
#endif
                    CallBack(e, resp.result(), resp.reason().to_string(), resp.body());
                }

                // in any case, clear the buffer and the response
                buffer.consume(buffer.size());
                boost::beast::http::response<boost::beast::http::string_body> emptyResp;
                boost::beast::http::swap(resp, emptyResp);
            });
    }

    detail::BoostAsioLib::ContextType& ios;
    const std::string host;
    const std::string port;
    const std::string auth;

    boost::asio::ip::tcp::resolver resolver;
    boost::asio::ip::tcp::socket socket;

    boost::beast::flat_buffer buffer; // (Must persist between reads)
    boost::beast::http::response<boost::beast::http::string_body> resp;
    boost::beast::http::request<boost::beast::http::string_body> request;

    std::queue<Request> pending;
#ifdef ARICPP_HTTP_TIMEOUT
    boost::asio::steady_timer timer;
#endif
};

} // namespace aricpp

#endif
