#include <cstdlib>

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include <boost/program_options.hpp>


struct Job
{
    enum StatusType
    {
        Running,
        Paused,
        Stopped
    };

    Job() : _status( Running ), _completed( false ) {}
    Job( const Job& j ) : _status( j._status ), _completed( j._completed ) {}
    Job& operator = ( const Job& j )
    {
        _status = j._status;
        _completed = j._completed;

        return *this;
    }

    Job( Job&& j )
    {
        _status = std::move( j._status );
        _completed = std::move( j._completed );
    }

    std::string toString() const
    {
        if( _status == Running )
        {
            return "Status : Running";
        }
        else if( _status == Paused )
        {
            return "Status : Paused";
        }
        else
        {
            return "Status : Stopped";
        }
    }

    void operator () ()
    {
        // doStuff();
        // _completed = true;
    }

    StatusType _status;
    bool _completed;

    std::mutex _mutex;
    std::condition_variable _condition;
};

struct Worker
{
    Worker( Job& j ) : _job( j ) {}
    ~Worker() = default;

    Worker( Worker&& ) = default;

    Worker() = delete;
    Worker( const Worker& ) = delete;
    void operator = ( const Worker& ) = delete;

    void operator () ()
    {
        while( true )
        {
            {
                std::unique_lock< std::mutex > lock( _job._mutex );

                while( _job._status == Job::Paused )
                {
                    _job._condition.wait( lock );
                }

                if( _job._completed )
                {
                    _job._condition.notify_all();
                    break;
                }

                if( _job._status == Job::Paused )
                {}

                if( _job._status == Job::Stopped )
                {
                    std::cout << "thread stopped" << std::endl;
                    break;
                }
            }

            _job();
        }

    }

    Job& _job;
};

class ThreadManager
{
public:
    ThreadManager( size_t n ) : _numberOfThreads( n ), _jobs( n ), _threads( n )
    {
    }

    ~ThreadManager() {}

    ThreadManager() = delete;
    ThreadManager( const ThreadManager& ) = delete;
    ThreadManager( ThreadManager&& ) = delete;
    void operator = ( const ThreadManager& ) = delete;


    void run()
    {
        for( size_t i = 0; i < _numberOfThreads; i++ )
        {
            _jobs[i] = Job();
            _threads[i] = std::thread( Worker( _jobs[i] ) );
        }

        std::cout << "start " << _numberOfThreads << " threads and wait for instructions" << std::endl;

        size_t n = _numberOfThreads;

        while( n != 0 )
        {
            std::string instruction;
            std::cin >> instruction;

            if( instruction.compare( "--help" ) == 0 )
            {
                std::cout << "list of commands" << std::endl;
                std::cout << "--status : print status of all threads" << std::endl;
                std::cout << "--stop-all : stop all threads" << std::endl;
                std::cout << "--stop <id> : stop thread <id>" << std::endl;
                std::cout << "--pause <id> : pause thread <id>" << std::endl;
                std::cout << "--restart <id> : restart paused thread <id>" << std::endl;
            }
            else if( instruction.compare( "--status" ) == 0 )
            {
                this->status( n );
            }
            else if( instruction.compare( "--stop-all" ) == 0 )
            {
                this->stopAll( n );
            }
            else
            {
                int ii;
                std::cin >> ii;
                --ii;

                if( ( ii >= 0 ) && ( ii < static_cast< int >( _numberOfThreads ) ) )
                {
                    size_t idx = static_cast< size_t >( ii );

                    if( instruction.compare( "--stop" ) == 0 )
                    {
                        this->stop( idx, n );
                    }
                    else if( instruction.compare( "--pause" ) == 0 )
                    {
                        this->pause( idx, n );
                    }
                    else if( instruction.compare( "--restart" ) == 0 )
                    {
                        this->restart( idx, n );
                    }
                }
                else
                {
                    std::cerr << "wrong id: id should be in [ 1, " << _numberOfThreads << " ]" << std::endl;
                }
            }
        }
    }

private:
    size_t _numberOfThreads;
    std::vector< Job > _jobs;
    std::vector< std::thread > _threads;

    void status( size_t& )
    {
        for( size_t i = 0; i < _jobs.size(); i++ )
        {
            std::cout << "Thread : " << i + 1 << ", " << _jobs[i].toString() << std::endl;
        }
    }

    void stopAll( size_t& n )
    {
        for( size_t i = 0; i < _jobs.size(); i++ )
        {
            this->stop( i, n );
        }
    }

    void stop( size_t id, size_t& n )
    {
        if( _jobs[id]._status != Job::Stopped )
        {
            {
                std::unique_lock< std::mutex > lock( _jobs[id]._mutex );
                _jobs[id]._status = Job::Stopped;
            }

            {
                _jobs[id]._condition.notify_one();
            }

            _threads[id].join();

            std::cout << "stopping thread " << id << std::endl;
            --n;

        }
    }

    void pause( size_t id, size_t& )
    {
        if( _jobs[id]._status == Job::Running )
        {
            {
                std::unique_lock< std::mutex > lock( _jobs[id]._mutex );
                _jobs[id]._status = Job::Paused;
                std::cout << "pausing thread " << id << std::endl;
            }

            {
                _jobs[id]._condition.notify_one();
            }
        }
    }

    void restart( size_t id, size_t& )
    {
        if( _jobs[id]._status == Job::Paused )
        {
            {
                std::unique_lock< std::mutex > lock( _jobs[id]._mutex );
                _jobs[id]._status = Job::Running;
                std::cout << "restarting thread " << id << std::endl;
            }
            {
                _jobs[id]._condition.notify_one();
            }
        }
    }

};

int main( int argc, char * argv[] )
{
    boost::program_options::options_description description( "options" );
    description.add_options()
    ( "help", "print help message and instructions" )
    (  "thread",
       boost::program_options::value< size_t >(),
       "number of threads to run" );

    try
    {
        boost::program_options::variables_map variableMap;
        boost::program_options::store(
            boost::program_options::parse_command_line( argc, argv, description ), variableMap );
        boost::program_options::notify( variableMap );

        size_t numberOfThreads = 0;

        if( variableMap.count( "help" ) != 0 )
        {
            std::cout << description << std::endl;
            return EXIT_FAILURE;
        }
        else if( variableMap.count( "thread" ) != 0 )
        {
            numberOfThreads = variableMap[ "thread" ].as< size_t >();
        }
        else
        {
            std::cout << description << std::endl;
            return EXIT_FAILURE;
        }

        ThreadManager manager( numberOfThreads );
        manager.run();
    }
    catch( boost::program_options::error& e )
    {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        std::cerr << description << std::endl;
        return EXIT_FAILURE;
    }
    catch(std::exception& e)
    {
        std::cerr << "Unhandled Exception reached the top of main: "
                  << e.what() << ", application will now exit" << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


