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


#ifndef ARICPP_CLIENT_H_
#define ARICPP_CLIENT_H_

#include <string>
#include <unordered_map>
#include <boost/asio.hpp>
#include <sstream>
#include <algorithm>

#include "websocket.h"
#include "httpclient.h"
#include "jsontree.h"

namespace aricpp
{

/**
 * @brief Client of a ARI connection.
 * 
 * This class can connect to the ARI interface of a asterisk box,
 * and can be used to send and receive raw messages to it.
 * You can use a high-level interface by instantiating an object
 * of class AriModel that use the Client object.
 */
class Client
{
public:

    using ConnectHandler = std::function< void(boost::system::error_code) >;
    using EventHandler = std::function< void(const JsonTree&) >;

    Client( boost::asio::io_service& ios, const std::string& host, const std::string& port,
            std::string _user, std::string _password, std::string _application ) :
        user(std::move(_user)), password(std::move(_password)),
        application(std::move(_application)),
        websocket(ios, host, port),
        httpclient(ios, host, port, user, password)
    {}

    Client() = delete;
    Client( const Client& ) = delete;
    Client( Client&& ) = delete;
    Client& operator = ( const Client& ) = delete;

    ~Client() noexcept
    {
        Close();
    }

    void Close() noexcept
    {
        websocket.Close();
    }

    /**
     * @brief Connect to asterisk via ARI
     * 
     * @param h the callback invoked when the library connect/disconnects to asterisk
     * @param connectionRetrySeconds the period in seconds of reconnection tries. When this parameter is 0 (default)
     * no reconnection is tried.
     */
    void Connect(ConnectHandler h, std::size_t connectionRetrySeconds = 0)
    {
        onConnection = std::move(h);
        websocket.Connect( "/ari/events?api_key="+user+":"+password+"&app="+application+"&subscribeAll=true", [this](auto e){
            if (e) onConnection(e);
            else this->WebsocketConnected(); // gcc requires "this"
        },
        connectionRetrySeconds );
    }

    void OnEvent( const std::string& type, const EventHandler& e )
    {
        eventHandlers.insert( std::make_pair( type, e ) );
    }

    template <typename ResponseHandler>
    void RawCmd(Method method, const std::string& url, ResponseHandler&& Response, std::string body={})
    {
        httpclient.SendRequest(method, url, std::forward<ResponseHandler>(Response), body);
    }

private:

    void WebsocketConnected()
    {
        websocket.Receive( [this](const std::string& msg, auto e){
            if ( e )
                std::cerr << "Error ws receive: " << e.message() << std::endl; // TODO remove print
            else
                this->RawEvent( msg ); // gcc requires this
        });
        // TODO: should become optional?
        httpclient.SendRequest( Method::post, "/ari/applications/"+application+"/subscription?eventSource=channel:,endpoint:,bridge:,deviceState:", [this](auto ec, auto, auto, auto){
            if ( ec ) std::cerr << ec.message() << std::endl; // TODO
            else onConnection( ec );
        });
    }

    void RawEvent( const std::string& msg )
    {
        try
        {
            JsonTree tree = FromJson( msg );
            const std::string type = Get<std::string>(tree, {"type"});
            auto range = eventHandlers.equal_range( type );
            std::for_each( range.first, range.second, [&tree,&type](auto f){
                try
                {
                    f.second(tree);
                }
                catch (const std::exception& e)
                {
                    // TODO
                    std::cerr << "Exception in handler of event: " << type << ": " << e.what() << '\n';
                }
                catch (...)
                {
                    // TODO
                    std::cerr << "Unknown exception in handler of event: " << type << '\n';
                }
            } );
        }
        catch ( const std::exception& e )
        {
            // TODO
            std::cerr << "Exception parsing " << msg << ": " << e.what() << std::endl;
        }
    }

    const std::string user;
    const std::string password;
    const std::string application;
    WebSocket websocket;
    HttpClient httpclient;
    ConnectHandler onConnection;

    using EventHandlers = std::unordered_multimap< std::string, EventHandler >;
    EventHandlers eventHandlers;
};

} // namespace aricpp

#endif
