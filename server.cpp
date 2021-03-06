#include "Poco/Net/TCPServer.h"
#include "Poco/Net/TCPServerConnection.h"
#include "Poco/Net/TCPServerConnectionFactory.h"
#include "Poco/Net/TCPServerParams.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketStream.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Timestamp.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/Exception.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/IntValidator.h"
#include "Poco/Util/HelpFormatter.h"
#include <algorithm>
#include <iostream>
#include <bitset>
#include <fstream>
#include <unordered_map>
#include <Poco/StreamCopier.h>
#include "hamming_code.h"


using Poco::Net::ServerSocket;
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


class HammingCodeServerConnection: public TCPServerConnection
    /// This class handles all client connections.
{
public:
    explicit HammingCodeServerConnection(const StreamSocket& s, const std::string& file, int connectionId)
        : TCPServerConnection(s)
        , file(file)
        , connectionId(connectionId) {
        buffer = new char[bufSize];
    }

    ~HammingCodeServerConnection() {
        delete[] buffer;
    }

    void run()
    {
        Application& app = Application::instance();
        try
        {
            int n = socket().receiveBytes(buffer, bufSize);
            while (n > 0)
            {
                curPos += n;
                decodeAvailableBlocks();
                n = socket().receiveBytes(buffer + curPos, bufSize - curPos);
            }
            std::string stat = Poco::format("detected errors: %d single, %d double, %d many", detected[1], detected[2], detected[-1]);
            app.logger().information("decoded message size: %z", decodedMessage.length());
            app.logger().information(stat);
            writeResult();
            std::cout << "written result" << std::endl;
            app.logger().information("will send answer %s", stat);
            int cur = 0;
            while (cur != stat.length()) {
                cur += socket().sendBytes(stat.data() + cur, stat.length() - cur);
            }
            app.logger().information("sent answer to connection %d", connectionId);
        }
        catch (Poco::Exception& exc)
        {
            std::cerr << "ClientConnection: " << exc.displayText() << std::endl;
        }
    }

    void decodeAvailableBlocks() {
        Application& app = Application::instance();
        int fullBlocks = curPos / hammingCode.getBlockSize();
        for (int blockIndex = 0; blockIndex < fullBlocks; blockIndex++) {
            char *blockStart = buffer + (blockIndex * hammingCode.getBlockSize());
            std::bitset<FixedHammingCode::getBlockSize()> block;
            for (int i = 0; i < hammingCode.getBlockSize(); i++) {
                if (blockStart[i] != '1' && blockStart[i] != '0') {
                    throw Poco::Exception(Poco::format("unknown char: %c", blockStart[i]));
                }
                block[i] = blockStart[i] == '1';
            }
            app.logger().debug("decoding block %s at %d", block.to_string(), curPos);
            auto decodingResult = hammingCode.decode(block);
            auto word = decodingResult.first.to_string();
            app.logger().debug("decoded to %s", word);
            detected[decodingResult.second] += 1;
            std::reverse(word.begin(), word.end());
            decodedMessage += word;
        }
        int decodedSize = fullBlocks * hammingCode.getBlockSize();
        for (int i = 0; i < curPos % hammingCode.getBlockSize(); i++) {
            buffer[i] = buffer[decodedSize + i];
        }
        curPos %= hammingCode.getBlockSize();
    }

    void writeResult() {
        Application& app = Application::instance();
        std::string binaryResult;
        auto lastWord = decodedMessage.substr(decodedMessage.length() - wordSize);
        std::reverse(lastWord.begin(), lastWord.end());
        std::bitset<wordSize> tailSizeWord(lastWord);
        size_t tailSize = tailSizeWord.to_ulong();
        app.logger().information("tail size: %z", tailSize);
        if (tailSize > wordSize - 1) {
            app.logger().information("bad tail size: %z", tailSize);
            tailSize = 0;
        }

        for (size_t i = 0; i < (decodedMessage.length() - tailSize - wordSize) / 8; i++) {
            auto c = decodedMessage.substr(i * 8, 8);
            reverse(c.begin(), c.end());
            std::bitset<8> cc(c);
            binaryResult.push_back((char) cc.to_ulong());
        }

        auto filename = Poco::format("%s_%d.txt", file, connectionId);
        app.logger().information("writing decoded message of size %z to %s", binaryResult.length(), filename);
        std::ofstream of(filename);
        of.write(binaryResult.data(), binaryResult.length());
        of.close();
    }

private:
    const FixedHammingCode hammingCode;
    const int bufSize = 100000000;
    char *buffer;
    int curPos = 0;
    const std::string& file;
    const int connectionId;
    std::string decodedMessage;
    size_t messageLength;
    std::unordered_map<int, int> detected;
};


class HammingCodeServerConnectionFactory: public TCPServerConnectionFactory
    /// A factory for HammingCodeServerConnection.
{
public:
    explicit HammingCodeServerConnectionFactory(const std::string& file): file(file) {
    }

    TCPServerConnection* createConnection(const StreamSocket& socket) final
    {
        return (TCPServerConnection *) new HammingCodeServerConnection(socket, file, lastConnectionId++);
    }

private:
    const std::string& file;
    static int lastConnectionId;
};

int HammingCodeServerConnectionFactory::lastConnectionId = 0;


class HammingCodeClient: public Poco::Util::ServerApplication
{
protected:
    void defineOptions(OptionSet& options)
    {
        ServerApplication::defineOptions(options);

        options.addOption(
            Option("help", "h", "display help information on command line arguments")
                .required(false)
                .repeatable(false)
                .binding("help"));

        options.addOption(
            Option("hostname", "H", "hostname to bind socket")
                .required(true)
                .repeatable(false)
                .argument("<hostname>", true)
                .binding("hostAddress"));

        options.addOption(
            Option("port", "p", "port to listen")
                .required(false)
                .repeatable(false)
                .argument("<port>", true)
                .binding("port")
                .validator(new Poco::Util::IntValidator(0, (1 << 16) - 1)));

        options.addOption(
            Option("file", "f", "output file name; connection id will be appended")
                .required(false)
                .repeatable(false)
                .argument("<file>", true)
                .binding("file"));
    }

    void handleOption(const std::string& name, const std::string& value)
    {
        ServerApplication::handleOption(name, value);

        if (name == "help") {
            stopOptionsProcessing();
        }
    }

    void displayHelp()
    {
        HelpFormatter helpFormatter(options());
        helpFormatter.setCommand(commandName());
        helpFormatter.setUsage("OPTIONS");
        helpFormatter.setHeader("A server application that uses Hamming code.");
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
            auto hostAddress = config().getString("hostAddress");
            unsigned short port = (unsigned short) config().getInt("port", 9911);
            app.logger().information("will bind to %s:%hu", hostAddress, port);

            // set-up a server socket
            ServerSocket svs(Poco::Net::SocketAddress(hostAddress, port));
            // set-up a TCPServer instance
            TCPServer srv(new HammingCodeServerConnectionFactory(config().getString("file")), svs);
            // start the TCPServer
            srv.start();
            // wait for CTRL-C or kill
            waitForTerminationRequest();
            // Stop the TCPServer
            srv.stop();
        }
        return Application::EXIT_OK;
    }

private:
};


int main(int argc, char** argv)
{
    HammingCodeClient app;
    return app.run(argc, argv);
}
