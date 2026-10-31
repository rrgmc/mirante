mirante_add_module(notes
    SOURCES   NotesView.cpp NotesView.h NotesModule.cpp
              Links.cpp Links.h LinksEditor.cpp LinksEditor.h LinksView.cpp LinksView.h
    RESOURCES icons/notes.svg icons/links.svg
    TESTS     tests/tst_register.cpp tests/tst_views.cpp tests/tst_links.cpp
    DEFAULT   ON)
