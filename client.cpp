#include "Poco/Config.h"
#undef POCO_EXCEPTION_BACKTRACE
#include "Poco/Net/TCPServer.h"
#include "Poco/Net/TCPServerConnection.h"
#include "Poco/Net/TCPServerConnectionFactory.h"
#include "Poco/Net/TCPServerParams.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Timestamp.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/Exception.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/NumberParser.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
#include <Poco/Util/IntValidator.h>
#include <Poco/Util/OptionException.h>
#include "hamming_code.h"


using Poco::Net::ServerSocket;
using Poco::Net::SocketAddress;
using Poco::Net::StreamSocket;
using Poco::Net::TCPServerConnection;
using Poco::Net::TCPServerConnectionFactory;
using Poco::Net::TCPServer;
using Poco::Timestamp;
using Poco::DateTimeFormatter;
using Poco::DateTimeFormat;
using Poco::Util::ServerApplication;
using Poco::Util::Application;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::HelpFormatter;


class HammingCodeClient: public Poco::Util::Application
{
protected:
    void defineOptions(OptionSet& options) override {
        Application::defineOptions(options);

        options.addOption(
            Option("help", "h", "display help information on command line arguments")
                .required(false)
                .repeatable(false)
                .binding("help"));

        options.addOption(
            Option("hostname", "H", "target server hostname")
                .required(true)
                .repeatable(false)
                .argument("<hostname>", true)
                .binding("hostnameAddress"));

        options.addOption(
            Option("port", "p", "target server port")
                .required(false)
                .repeatable(false)
                .binding("port")
                .validator(new Poco::Util::IntValidator(0, (1 << 16) - 1)));

        options.addOption(
            Option("file", "f", "file to send")
                .required(true)
                .repeatable(false)
                .argument("<file>", true)
                .binding("file"));

        options.addOption(
            Option("errors", "e", "how many errors to add")
                .required(false)
                .repeatable(false)
                .argument("<number>", true)
                .binding("errors")
                .validator(new Poco::Util::IntValidator(0, wordSize)));
    }

    virtual void handleOption(const std::string& name, const std::string& value) {
        Application::handleOption(name, value);

        if (name == "help") {
            stopOptionsProcessing();
        }
    }

    void displayHelp()
    {
        HelpFormatter helpFormatter(options());
        helpFormatter.setCommand(commandName());
        helpFormatter.setUsage("OPTIONS");
        helpFormatter.setHeader("A client application that sends message encoded with Hamming code.");
        helpFormatter.format(std::cout);
    }

    int main(const std::vector<std::string>& args)
    {
        auto& app = Application::instance();
        if (config().hasOption("help"))
        {
            displayHelp();
        }
        else
        {
            auto hostname = config().getString("hostnameAddress");
            unsigned short port = (unsigned short) config().getInt("port", 9911);

            app.logger().information("connecting to %s:%hu", hostname, port);

            StreamSocket socket(SocketAddress(hostname, port));

            auto filename = config().getString("file");
            app.logger().information("reading data from %s", filename);
            std::ifstream messageFile(filename);
            std::stringstream buffer;
            buffer << messageFile.rdbuf();
            auto encoded = encodeMessage(buffer.str());

            app.logger().debug("sending %s", encoded);
            int n = socket.sendBytes(encoded.data(), (int) encoded.length());
            app.logger().information("sent %d", n);
        }
        return Application::EXIT_OK;
    }

    std::string encodeMessage(const std::string& message) {
        std::string textMessage;
        for (auto c : message) {
            std::bitset<8> b((unsigned long long) c);
            auto s = b.to_string();
            std::reverse(s.begin(), s.end());
            textMessage += s;
        }
        size_t tail = wordSize - (textMessage.length() % wordSize);
        for (int i = 0; i < tail; i++) {
            textMessage.push_back('0');
        }
        Application::instance().logger().debug("text message: %s", textMessage);
        std::string encoded;
        for (int i = 0; i < textMessage.length() / wordSize; i++) {
            auto word = textMessage.substr(i * wordSize, wordSize);
            std::reverse(word.begin(), word.end());
            Application::instance().logger().debug("word: %s", word);
            std::bitset<wordSize> b(word);
            auto result = hammingCode.encode(b);
            for (int j = 0; j < config().getInt("errors", 0); j++) {
                int pos = rand() % hammingCode.getBlockSize();
                result.flip(pos);
            }
            auto encodedWord = result.to_string();
            Application::instance().logger().debug("encoded block: %s", encodedWord);
            std::reverse(encodedWord.begin(), encodedWord.end());
            encoded += encodedWord;
        }
        return encoded;
    }
private:
    const FixedHammingCode hammingCode;
};


int main(int argc, char** argv)
{
    HammingCodeClient client;
    try {
        client.init(argc, argv);
        return client.run();
    }
    catch (const Poco::Exception& e) {
        std::cerr << e.displayText() << std::endl;
    }
    return 1;
}
