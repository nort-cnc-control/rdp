#include <Python.h>
#include <rdpos.h>

static PyObject* create_connection(PyObject* self, PyObject* args)
{
    PyObject* connection;
    return Py_None;
}

static PyMethodDef myMethods[] = {
    { "create_connection", create_connection, METH_NOARGS, "Create new connection" },
    { NULL, NULL, 0, NULL }
};

static struct PyModuleDef myModule = {
    PyModuleDef_HEAD_INIT,
    "rdpos",
    "RDPoS communication module",
    -1,
    myMethods
};


PyMODINIT_FUNC PyInit_rdpos(void)
{
    return PyModule_Create(&myModule);
}
