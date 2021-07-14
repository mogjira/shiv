#find_package(obsidian REQUIRED)

include(author_library)

author_library(shiv 
    EXPORT_NAME Shiv
    SOURCES shiv.c
    PUBLIC_HEADERS shiv.h
    DEPS Obsidian::Obsidian Hell::Hell)

author_library(shivObj
    TYPE OBJECT
    EXPORT_NAME ShivObj
    SOURCES shiv.c
    DEPS Obsidian::ObsidianObj Hell::HellObj)
