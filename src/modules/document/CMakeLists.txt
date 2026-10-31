mirante_add_module(document
    SOURCES   DocumentModule.cpp
              DocumentEditor.cpp DocumentEditor.h
              DocumentFormat.cpp DocumentFormat.h
              DocumentLinks.cpp DocumentLinks.h
              DocumentSource.cpp DocumentSource.h
              DocumentView.cpp DocumentView.h
    RESOURCES icons/document-url.svg icons/document-file.svg icons/document-project.svg
    TESTS     tests/tst_register.cpp tests/tst_links.cpp tests/tst_views.cpp
    QT_LIBS   Network Pdf PdfWidgets
    DEFAULT   ON)
