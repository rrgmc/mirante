mirante_add_module(folder
    SOURCES   FolderView.cpp FolderView.h FolderModule.cpp
              FileTreeView.cpp FileTreeView.h StartEditor.cpp StartEditor.h
    RESOURCES icons/folder.svg icons/tree.svg
    TESTS     tests/tst_register.cpp tests/tst_views.cpp tests/tst_file_tree.cpp
    DEFAULT   ON)
