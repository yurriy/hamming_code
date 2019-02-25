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
#include "Poco/Util/HelpFormatter.h"
#include <algorithm>
#include <iostream>
#include <bitset>
#include <fstream>
#include <unordered_map>
#include <Poco/StreamCopier.h>
#include "hamming_code.h"
#include "hamming_code_word_size.h"


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
        std::string result;
        StreamSocket& ss = socket();
        std::unordered_map<int, int> detectedErrors;
        try
        {
            int n = ss.receiveBytes(buffer, bufSize);
            while (n > 0)
            {
                curPos += n;
                int fullBlocks = curPos / hammingCode.getBlockSize();
                for (int blockIndex = 0; blockIndex < fullBlocks; blockIndex++) {
                    char *blockStart = buffer + (blockIndex * hammingCode.getBlockSize());
                    std::bitset<hammingCode.getBlockSize()> block;
                    for (int i = 0; i < hammingCode.getBlockSize(); i++) {
                        if (blockStart[i] != '1' && blockStart[i] != '0') {
                            throw Poco::Exception(std::string("unknown char: ") + std::to_string(blockStart[i]));
                        }
                        block[i] = blockStart[i] == '1';
                    }
                    app.logger().debug(std::string("decoding block ") + block.to_string() + " at " + std::to_string(curPos));
                    auto decodingResult = hammingCode.decode(block);
                    auto word = decodingResult.first.to_string();
                    app.logger().debug(std::string("decoded to ") + word);
                    detectedErrors[decodingResult.second] += 1;
                    std::reverse(word.begin(), word.end());
                    result += word;
                }
                int decodedSize = fullBlocks * hammingCode.getBlockSize();
                for (int i = 0; i < curPos % hammingCode.getBlockSize(); i++) {
                    buffer[i] = buffer[decodedSize + i];
                }
                curPos %= hammingCode.getBlockSize();
                n = ss.receiveBytes(buffer + curPos, bufSize - curPos);
            }
            app.logger().information(std::string("detected single errors: ") + std::to_string(detectedErrors[1]));
            app.logger().information(std::string("detected double errors: ") + std::to_string(detectedErrors[2]));
            app.logger().information(std::string("detected many errors: ") + std::to_string(detectedErrors[-1]));

            app.logger().debug(std::string("decoded result size: ") + std::to_string(result.length()));
            app.logger().debug(std::string("decoded result: ") + result);
            std::string binaryResult;
            for (size_t i = 0; i < result.length() / 8; i++) {
                auto c = result.substr(i * 8, 8);
                reverse(c.begin(), c.end());
                std::bitset<8> cc(c);
                binaryResult.push_back((char) cc.to_ulong());
            }
            auto filename = file + std::to_string(connectionId);
            app.logger().information(std::string("writing decoded message to ") + filename);
            std::ofstream of(filename);
            of.write(binaryResult.data(), binaryResult.length());
        }
        catch (Poco::Exception& exc)
        {
            std::cerr << "ClientConnection: " << exc.displayText() << std::endl;
        }
    }

private:
    static constexpr HammingCode<wordSize> hammingCode = HammingCode<wordSize>();
    const int bufSize = 100000000;
    char *buffer;
    int curPos = 0;
    const std::string& file;
    const int connectionId;
};


class HammingCodeServerConnectionFactory: public TCPServerConnectionFactory
    /// A factory for HammingCodeServerConnection.
{
public:
    HammingCodeServerConnectionFactory(const std::string& file): file(file) {
    }

    TCPServerConnection* createConnection(const StreamSocket& socket)
    {
        return new HammingCodeServerConnection(socket, file, lastConnectionId++);
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
                .repeatable(false));

        options.addOption(
            Option("bind", "b", "hostname to bind socket")
                .required(false)
                .repeatable(false));

        options.addOption(
            Option("file", "f", "file name prefix")
                .required(false)
                .repeatable(false)
                .argument("<file>", true));
    }

    void handleOption(const std::string& name, const std::string& value)
    {
        ServerApplication::handleOption(name, value);

        if (name == "help")
            _helpRequested = true;
        if (name == "file")
            file = value;
    }

    void displayHelp()
    {
        HelpFormatter helpFormatter(options());
        helpFormatter.setCommand(commandName());
        helpFormatter.setUsage("OPTIONS");
        helpFormatter.setHeader("A server application that uses Hamming Code.");
        helpFormatter.format(std::cout);
    }

    int main(const std::vector<std::string>& args)
    {
        if (_helpRequested)
        {
            displayHelp();
        }
        else
        {
            // get parameters from configuration file
            unsigned short port = (unsigned short) config().getInt("HammingCodeClient.port", 9911);

            // set-up a server socket
            ServerSocket svs(port);
            // set-up a TCPServer instance
            TCPServer srv(new HammingCodeServerConnectionFactory(file), svs);
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
    bool _helpRequested = false;
    std::string file = "default_file";
};


int main(int argc, char** argv)
{
    HammingCodeClient app;
    return app.run(argc, argv);
}