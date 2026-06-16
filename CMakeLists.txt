cmake_minimum_required(VERSION 4.1)
project(Lumen C)

set(CMAKE_C_STANDARD 23)

add_executable(Lumen main.c
        common.h
        chunk.h
        chunk.c
        memory.h
        memory.c
        debug.c
        debug.h
        value.c
        value.h
        vm.c
        vm.h
        compiler.c
        compiler.h
        scanner.c
        scanner.h
        object.h
        object.c
        table.h
        table.c
        table.c
        main.c
        native.c
        native.h
        native.h
        native.c
        native.h
        native.h

)
