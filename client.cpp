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


class ProbabilityValidator: public Poco::Util::Validator {
public:
    void validate(const Option& option, const std::string& value) {
        double parsed;
        if (!Poco::NumberParser::tryParseFloat(value, parsed)) {
            throw Poco::Util::OptionException(Poco::format("failed to parse float from %s option parameter: %s", option.fullName(), value));
        }
        if (parsed < 0 || parsed > 1) {
            throw Poco::Util::OptionException(Poco::format("%s value %s is not in range [0, 1]", option.fullName(), value));
        }
    }
};


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
            Option("error-prob", "P", "probability of error in word, float")
                .required(false)
                .repeatable(false)
                .argument("<float>", true)
                .binding("error-prob")
                .validator(new ProbabilityValidator()));

        options.addOption(
            Option("error-count", "e", "number of errors in one word")
                .required(false)
                .repeatable(false)
                .argument("<number>", true)
                .binding("error-count")
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
            addErrors(encoded);

            app.logger().debug("sending %d bytes", encoded.length());
            size_t cur = 0;
            while (cur != encoded.length()) {
                cur += socket.sendBytes(encoded.data() + cur, encoded.length() - cur);
            }
            std::cout << "send finished" << std::endl;
            socket.shutdownSend();
            char serverAnswer[1000];
            cur = 0;
            int n = socket.receiveBytes(serverAnswer + cur, sizeof(serverAnswer) - cur);
            while (n > 0) {
                cur += n;
                n = socket.receiveBytes(serverAnswer + cur, sizeof(serverAnswer) - cur);
            }
            app.logger().information("server answer: %s", std::string(serverAnswer, cur));
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

        std::string tailSizeBlock = std::bitset<wordSize>(tail).to_string();
        std::reverse(tailSizeBlock.begin(), tailSizeBlock.end());
        textMessage += tailSizeBlock;

        std::string encoded;
        for (size_t i = 0; i < textMessage.length() / wordSize; i++) {
            auto word = textMessage.substr(i * wordSize, wordSize);
            std::reverse(word.begin(), word.end());
            std::bitset<wordSize> b(word);
            auto encodedWord = hammingCode.encode(b).to_string();
            std::reverse(encodedWord.begin(), encodedWord.end());
            encoded += encodedWord;
        }
        return encoded;
    }

    void addErrors(std::string& data) {
        double errorProb = config().getDouble("error-prob", 1);
        int errorCount = config().getInt("error-count", 0);
        int blockSize = hammingCode.getBlockSize();
        logger().information("blocks: %d", (int) (data.length() / blockSize));
        int count = 0;
        for (size_t i = 0; i < data.length() / blockSize; i++) {
            if (((double) rand() / RAND_MAX) > errorProb) {
                continue;
            }
            for (size_t j = 0; j < errorCount; j++) {
                size_t pos = i * blockSize + (rand() % blockSize);
                data[pos] = data[pos] == '0' ? '1' : '0';
                count++;
            }
        }
        logger().information("added errors: %d", count);
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
