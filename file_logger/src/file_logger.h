#ifndef FILE_LOGGER_H
#define FILE_LOGGER_H

#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <pwd.h>

#include <chrono>


std::string get_current_time_and_date()
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
    return ss.str();
}

#define MAX_EVENTS 1024
#define LEN_NAME 16 
#define EVENT_SIZE  ( sizeof (struct inotify_event) ) 
#define BUF_LEN     ( MAX_EVENTS * ( EVENT_SIZE + LEN_NAME ))


namespace Logger
{
    class Timestamp{

    private:
        typedef std::chrono::high_resolution_clock::time_point t_point;
        const t_point t_stamp;
    public:
        Timestamp():t_stamp(std::chrono::high_resolution_clock::now()){}
        
        int getEllapsedTime() const{
            std::chrono::duration<double> duration = std::chrono::high_resolution_clock::now() - t_stamp;
            return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        }

    };

    class Logger
    {
        using Descriptor = int;
        using INotifyEvent = struct inotify_event;

    private:
        Descriptor      _fd;
        std::ofstream   _output_file; 
        int             _wd;
        Timestamp       _timestamp;
        int             _timelimit;
        std::string     _trackedFile;


    public:
        Logger() = default;
        Logger(std::string trackedFile, std::string outputFile = "LogFile.txt", int timelimit = 0)
            : _fd(inotify_init1(0)), _timestamp(Timestamp()), _timelimit(timelimit), _trackedFile(trackedFile)
        {
            _output_file.open(outputFile, std::ios::app);
            _output_file << "Logging the " << trackedFile << "..." << std::endl;
            _output_file << std::setw(40) << std::left << "DESCRIPTION" <<  std::setw(40) << std::left << "TIME" << std::setw(40) << std::left << "USER" << std::endl;

            if (_fd < 0)
                perror( "inotify_init" );

            _wd = inotify_add_watch( _fd, trackedFile.c_str(), IN_ALL_EVENTS );

            if (_wd == -1) perror( "Couldn't add file to track" );
        }

        std::string getEventType(INotifyEvent* event)
        {
            if(event->mask & IN_ACCESS)
                return "file was read";
            if(event->mask & IN_ATTRIB)
                return "metadata modified";
            if(event->mask & IN_CLOSE_WRITE)
                return "file was open and closed for write";
            if(event->mask & IN_CREATE)
                return "file created inside tracked catalog";
            if(event->mask & IN_DELETE)
                return "file deleted from tracked catalog";
            if(event->mask & IN_DELETE_SELF)
                return "file was deleted";
            if(event->mask & IN_MODIFY)
                return "file modified";
            if((event->mask & IN_MOVE_SELF) || (event->mask & IN_MOVED_FROM) || (event->mask & IN_MOVED_TO))
                return "file moved";
            if(event->mask & IN_OPEN)
                return "file opened";
            return "Unknown action";
        }

        void Run()
        {
            int status, length = 0;
            char buffer[BUF_LEN];

            pid_t pid;
            switch(pid = fork()){
            case 0:
                while(true)
                {   
                    int i = 0;
                    length = read(_fd, buffer, BUF_LEN);
                    
                    if (length < 0) perror("Couldn't read the fd");

                    while(i < length)
                    {
                        struct inotify_event* event = (struct inotify_event*) &buffer[i];
                        struct stat fileInfo;
                        stat(_trackedFile.c_str(), &fileInfo);
                        _output_file << std::setw(40) << std::left << getEventType(event) << std::setw(40) << std::left << get_current_time_and_date().c_str() << std::setw(40) << std::left << getpwuid(fileInfo.st_uid)->pw_name << std::endl;
                        i += EVENT_SIZE + event->len;
                    }
                }

            default:
                if(_timelimit != 0){
                    while(_timestamp.getEllapsedTime() < 1000*_timelimit){}
                    kill(pid, SIGTERM);
                }
            }
            waitpid(pid, &status, 0);    
        }

        ~Logger()
        { 
            inotify_rm_watch(_fd, _wd);
            close(_fd);
            _output_file.close();
        }
    };

    class LoggerKeeper
    {
    public:
        static Logger* _logger;
    public:
        LoggerKeeper() = default;
        static void deleteLogger(int sig) { delete LoggerKeeper::_logger; exit(0); }
    };

    Logger* LoggerKeeper::_logger = new Logger();
}

#endif