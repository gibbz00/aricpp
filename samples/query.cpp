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


#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include "../aricpp/client.h"

using namespace aricpp;

int main(int argc, char* argv[])
{
    try
    {

        std::string host = "localhost";
        std::string port = "8088";
        std::string username = "asterisk";
        std::string password = "asterisk";

        namespace po = boost::program_options;
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "produce help message")
            ("version,V", "print version string")

            ("host,H", po::value(&host), "ip address of the ARI server [localhost]")
            ("port,P", po::value(&port), "port of the ARI server [8088]")
            ("username,u", po::value(&username), "username of the ARI account on the server [asterisk]")
            ("password,p", po::value(&password), "password of the ARI account on the server [asterisk]")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            std::cout << desc << "\n";
            return 0;
        }

        if (vm.count("version"))
        {
            std::cout << "This is query application v. 1.0, part of aricpp library\n";
            return 0;
        }

        boost::asio::io_service ios;
        HttpClient client( ios, host, port, username, password );

        std::vector<std::string> requests
        {
            "/ari/asterisk/info",
            "/ari/asterisk/modules",
            "/ari/asterisk/logging",
            "/ari/applications",
            "/ari/bridges",
            "/ari/channels",
            "/ari/deviceStates",
            "/ari/endpoints",
            "/ari/mailboxes",
            "/ari/recordings/stored",
            "/ari/sounds"
        };

        std::for_each( requests.begin(), requests.end(), [&client](auto& request){
            client.SendRequest( Method::get, request, [&client,&request](auto error, auto state, auto reason, auto body ){
                std::cout << "\nREQUEST " << request << ":\n"
                          << "error: " << error.message() << '\n'
                          << "state: " << state << '\n'
                          << "reason: " << reason << '\n'
                          << body << '\n';
            } );

        } );

        ios.run();

        return 0;
    }
    catch ( const std::exception& e )
    {
        std::cerr << "Error: " << e.what() << '\n';
        return -1;
    }

    return 0;
}

