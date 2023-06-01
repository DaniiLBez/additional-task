#include <iostream>
#include <string>
#include "file_logger.h"


int main(int argc, char** argv) {

    std::string file;
    int timelimit = 0;

    if(argc < 2){
        std::cout << "No file to track\n";
        return -1;
    }else{
        if(argc == 2){
            file = argv[1];
        }else{
            file = argv[1];
            timelimit = atoi(argv[2]);
        }
    }

    signal(SIGINT, Logger::LoggerKeeper::deleteLogger);

    Logger::Logger* logger = new Logger::Logger(file, "LogFile.txt", timelimit);
    std::swap(Logger::LoggerKeeper::_logger, logger);
    
    Logger::LoggerKeeper::_logger->Run();

    delete Logger::LoggerKeeper::_logger;

    return 0;
}
