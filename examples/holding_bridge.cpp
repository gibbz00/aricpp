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

#include <iostream>
#include <string>
#include <vector>
#include <boost/program_options.hpp>

#include "../include/aricpp/arimodel.h"
#include "../include/aricpp/bridge.h"
#include "../include/aricpp/channel.h"
#include "../include/aricpp/client.h"

using namespace aricpp;
using namespace std;

inline std::string to_string(bool b)
{
    return (b ? "true" : "false");
}

int main(int argc, char* argv[])
{
    try
    {
        string host = "localhost";
        string port = "8088";
        string username = "asterisk";
        string password = "asterisk";
        string application = "attendant";
        bool sipCh = false; // default = pjsip channel

        namespace po = boost::program_options;
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "produce help message")
            ("version,V", "print version string")

            ("host,H", po::value(&host), ("ip address of the ARI server ["s + host + ']').c_str())
            ("port,P", po::value(&port), ("port of the ARI server ["s + port + "]").c_str())
            ("username,u", po::value(&username), ("username of the ARI account on the server ["s + username + "]").c_str())
            ("password,p", po::value(&password), ("password of the ARI account on the server ["s + password + "]").c_str())
            ("application,a", po::value(&application), ("stasis application to use ["s + application + "]").c_str())
            ("sip-channel,S", po::bool_switch(&sipCh), ("use old sip channel instead of pjsip channel ["s + to_string(sipCh) + "]").c_str())
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            cout << desc << "\n";
            return 0;
        }

        if (vm.count("version"))
        {
            cout << "This is dial holding_bridge v. 1.0, part of aricpp library\n";
            return 0;
        }

#if BOOST_VERSION < 106600
        using IoContext = boost::asio::io_service;
#else
        using IoContext = boost::asio::io_context;
#endif
        IoContext ios;

        // Register to handle the signals that indicate when the server should exit.
        // It is safe to register for the same signal multiple times in a program,
        // provided all registration for the specified signal is made through Asio.
        boost::asio::signal_set signals(ios);
        signals.add(SIGINT);
        signals.add(SIGTERM);
#if defined(SIGQUIT)
        signals.add(SIGQUIT);
#endif // defined(SIGQUIT)
        signals.async_wait(
            [&ios](boost::system::error_code /*ec*/, int /*signo*/)
            {
                cout << "Cleanup and exit application...\n";
                ios.stop();
            });

        vector<shared_ptr<Channel>> channels;

        Client client(ios, host, port, username, password, application);
        AriModel model(client);
        shared_ptr<Bridge> bridge;
        model.CreateBridge(
            [&bridge](unique_ptr<Bridge> newBridge)
            {
                if (!newBridge) return;

                bridge = move(newBridge);
                cout << "Bridge created" << endl;
            },
            Bridge::Type::holding);
        model.OnStasisStarted(
            [&channels, &bridge](shared_ptr<Channel> ch, bool external)
            {
                if (external)
                {
                    ch->Answer();
                    if (channels.empty())
                    {
                        cout << "Adding announcer to bridge" << endl;
                        bridge->Add(*ch, false /* mute */, Bridge::Role::announcer);
                    }
                    else
                    {
                        cout << "Adding participant to bridge" << endl;
                        bridge->Add(*ch, false /* mute */, Bridge::Role::participant);
                    }
                    channels.push_back(ch);
                }
                else
                {
                    cerr << "WARNING: should not reach this line" << endl;
                }
            });

        client.Connect(
            [&](boost::system::error_code e)
            {
                if (e)
                {
                    cerr << "Connection error: " << e.message() << endl;
                    ios.stop();
                }
                else
                    cout << "Connected" << endl;
            });

        ios.run();
    }
    catch (exception& e)
    {
        cerr << "Exception in app: " << e.what() << ". Aborting\n";
        return -1;
    }
    return 0;
}
