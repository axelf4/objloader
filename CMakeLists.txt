cmake_minimum_required (VERSION 2.8)
project(objloader C)

include_directories(include)
add_library(objloader STATIC include/objloader.h objloader.c)
