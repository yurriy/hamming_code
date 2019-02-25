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
#include "hamming_code.h"
#include "hamming_code_word_size.h"


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
    void defineOptions(OptionSet& options)
    {
        Application::defineOptions(options);

        options.addOption(
            Option("help", "h", "display help information on command line arguments")
                .required(false)
                .repeatable(false));

        options.addOption(
            Option("hostname", "H", "target server hostname")
                .required(false)
                .repeatable(false)
                .argument("<hostname>", true));

        options.addOption(
            Option("port", "p", "target server port")
                .required(true)
                .repeatable(false)
                .argument("<port>", true));

        options.addOption(
            Option("file", "f", "file with message")
                .required(true)
                .repeatable(false)
                .argument("<file>", true));

        options.addOption(
            Option("errors", "e", "error count")
                .required(true)
                .repeatable(false)
                .argument("<errors>", true));
    }

    void handleOption(const std::string& name, const std::string& value)
    {
        Application::handleOption(name, value);

        if (name == "help")
            _helpRequested = true;
        if (name == "hostname")
            hostname = value;
        if (name == "port")
            port = value;
        if (name == "file")
            file = value;
        if (name == "errors")
            errorCount = Poco::NumberParser::parse(value);
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
        if (_helpRequested)
        {
            displayHelp();
        }
        else
        {
            app.logger().information(std::string("connecting to ") + hostname + ':' + port);
            StreamSocket socket1(SocketAddress(hostname, port));

            std::ifstream messageFile(file);
            std::stringstream buffer;
            buffer << messageFile.rdbuf();
            auto encoded = encodeMessage(buffer.str());

            app.logger().debug(std::string("sending ") + encoded);
            int n = socket1.sendBytes(encoded.data(), (int) encoded.length());
            app.logger().information(std::string("sent ") + std::to_string(n));
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
        Application::instance().logger().debug(std::string("text message: ") + textMessage);
        std::string encoded;
        for (int i = 0; i < textMessage.length() / wordSize; i++) {
            auto word = textMessage.substr(i * wordSize, wordSize);
            std::reverse(word.begin(), word.end());
            Application::instance().logger().debug(std::string("word: ") + word);
            std::bitset<wordSize> b(word);
            auto result = hammingCode.encode(b);
            for (int j = 0; j < errorCount; j++) {
                int pos = rand() % hammingCode.getBlockSize();
                result.flip(pos);
            }
            auto encodedWord = result.to_string();
            Application::instance().logger().debug(std::string("encoded block: ") + encodedWord);
            std::reverse(encodedWord.begin(), encodedWord.end());
            encoded += encodedWord;
        }
        return encoded;
    }
private:
    int errorCount = 0;
    bool _helpRequested = false;
    std::string hostname = "localhost", port = "9911", file;
    static constexpr HammingCode<33> hammingCode = HammingCode<33>();
};


int main(int argc, char** argv)
{
    HammingCodeClient client;
    client.init(argc, argv);
    return client.run();
}
