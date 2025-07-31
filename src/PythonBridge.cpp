#include "PythonBridge.h"
#include <Python.h>
#include <iostream>

namespace PythonBridge {

void runTranscription() {
    Py_Initialize();
    if (!Py_IsInitialized()) {
        std::cerr << "Failed to initialize Python" << std::endl;
        return;
    }

    PyRun_SimpleString("import realtime_transcribe\nrealtime_transcribe.run()");
    Py_FinalizeEx();
}

} // namespace PythonBridge
