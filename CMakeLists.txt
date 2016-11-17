cmake_minimum_required (VERSION 2.8)
project(objparser C)

include_directories(include)
add_library(objparser STATIC include/objparser.h objparser.c)
