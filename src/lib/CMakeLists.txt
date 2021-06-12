#find_package(obsidian REQUIRED)

include(author_library)

author_library(shiv 
    EXPORT_NAME Shiv
    SOURCES shiv.c
    PUBLIC_HEADERS shiv.h
    DEPS Obsidian::Obsidian Hell::Hell)
