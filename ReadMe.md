$ mkdir -p build && cd build
$ ccmake -G Ninja ..
$ ninja

or

$ g++ -Wall -Wextra -pthread -std=gnu++11 main.cxx -o main -lboost_program_options
