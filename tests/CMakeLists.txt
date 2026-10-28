# Each tst_*.cpp file in a folder becomes one test, linked to that folder's library.
# Module tests are added by mirante_add_module().
mirante_add_tests(core LIBS mirante_core)
mirante_add_tests(items LIBS mirante_items)
mirante_add_tests(layout LIBS mirante_layout)
mirante_add_tests(ui LIBS mirante_ui)
