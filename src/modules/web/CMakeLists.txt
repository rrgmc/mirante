mirante_add_module(web
    SOURCES   WebModule.cpp WebUrls.cpp WebUrls.h
    RESOURCES icons/web.svg
    TESTS     tests/tst_register.cpp tests/tst_urls.cpp
    DEFAULT   ON)
