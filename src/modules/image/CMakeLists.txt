mirante_add_module(image
    SOURCES   ImageView.cpp ImageView.h ImageModule.cpp
    RESOURCES icons/image.svg
    TESTS     tests/tst_register.cpp tests/tst_views.cpp
    DEFAULT   ON)
