#include <Python.h>
#include <rdp.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define CLOCK_PERIOD_US 1000

#define SET_CB(conn, cb_name, val) do {  \
        if (conn->cb_name != Py_None)    \
            Py_DECREF(conn->cb_name);    \
        conn->cb_name = val;             \
        if (conn->cb_name != Py_None)    \
            Py_INCREF(conn->cb_name);    \
    } while(0)

#define CLEAR_CB(conn, cb)  SET_CB(conn, cb, Py_None)


struct py_rdp_connection_s {
    struct rdp_connection_s connection;
    uint8_t rdp_recv_buf[RDP_MAX_SEGMENT_SIZE];
    uint8_t rdp_outbuf[RDP_MAX_SEGMENT_SIZE];
    size_t recv_len;
    size_t send_len;
    struct {
        PyObject *data_received_cb;
        PyObject *data_transmitted_cb;
        PyObject *connected_cb;
        PyObject *closed_cb;
        PyObject *dgram_send_cb;
    };
};


static void add_connection_to_list(PyObject *c)
{
}

static void remove_connection_from_list(PyObject *c)
{
}

static void clear_list(void)
{
}

static void make_clock_tick(void)
{
    
}

static void _rdp_destroy_connection(PyObject *obj)
{
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(obj, "RDP Connection");
    if (conn == NULL)
        return;
    CLEAR_CB(conn, connected_cb);
    CLEAR_CB(conn, closed_cb);
    CLEAR_CB(conn, data_received_cb);
    CLEAR_CB(conn, data_transmitted_cb);
    CLEAR_CB(conn, dgram_send_cb);
    remove_connection_from_list(obj);
    free(conn);
}

static void connected_cb(struct rdp_connection_s *c)
{
    PyObject *connection = c->user_arg;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn->connected_cb != Py_None)
    {
        PyObject *args = Py_BuildValue("(O)", connection);
        PyObject_CallObject(conn->connected_cb, args);
        Py_DECREF(args);
    }
}

static void closed_cb(struct rdp_connection_s *c)
{
    PyObject *connection = c->user_arg;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn->closed_cb != Py_None)
    {
        PyObject *args = Py_BuildValue("(O)", connection);
        PyObject_CallObject(conn->closed_cb, args);
        Py_DECREF(args);
    }
}

static void data_transmitted_cb(struct rdp_connection_s *c)
{
    PyObject *connection = c->user_arg;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn->data_transmitted_cb != Py_None)
    {
        PyObject *args = Py_BuildValue("(O)", connection);
        PyObject_CallObject(conn->data_transmitted_cb, args);
        Py_DECREF(args);
    }
}

static void data_received_cb(struct rdp_connection_s *c, const uint8_t *d, size_t l)
{
    PyObject *connection = c->user_arg;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn->data_received_cb != Py_None)
    {
        PyObject *args = Py_BuildValue("(Oy#)", connection, d, l);
        PyObject_CallObject(conn->data_received_cb, args);
        Py_DECREF(args);
    }
}

static void dgram_send_cb(struct rdp_connection_s *c, const uint8_t *d, size_t l)
{
    PyObject *connection = c->user_arg;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn->dgram_send_cb != Py_None)
    {
        PyObject *args = Py_BuildValue("(Oy#)", connection, d, l);
        PyObject_CallObject(conn->dgram_send_cb, args);
        Py_DECREF(args);
    }
}

static PyObject* py_rdp_create_connection(PyObject* self, PyObject* args)
{
    struct py_rdp_connection_s *conn = malloc(sizeof(struct py_rdp_connection_s));
    if (conn == NULL)
        return NULL;
    memset(conn, 0, sizeof(*conn));
    rdp_init_connection(&conn->connection, conn->rdp_outbuf, conn->rdp_recv_buf);

    conn->connected_cb = Py_None;
    conn->closed_cb = Py_None;
    conn->data_transmitted_cb = Py_None;
    conn->data_received_cb = Py_None;
    conn->dgram_send_cb = Py_None;

    rdp_set_connected_cb(&conn->connection, connected_cb);
    rdp_set_closed_cb(&conn->connection, closed_cb);
    rdp_set_data_received_cb(&conn->connection, data_received_cb);
    rdp_set_data_send_completed_cb(&conn->connection, data_transmitted_cb);
    rdp_set_send_cb(&conn->connection, dgram_send_cb);

    PyObject* connection = PyCapsule_New(conn, "RDP Connection", _rdp_destroy_connection);
    rdp_set_user_argument(&conn->connection, connection);
    add_connection_to_list(connection);
    return connection;
}

static PyObject* py_rdp_state(PyObject* self, PyObject* args)
{
    PyObject *connection;
    if (!PyArg_ParseTuple(args, "O", &connection))
        return NULL;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn == NULL)
        return NULL;
    const char *state = NULL;
    switch (conn->connection.state)
    {
    case RDP_CLOSED:
        state = "CLOSED";
        break;
    case RDP_LISTEN:
        state = "LISTEN";
        break;
    case RDP_SYN_SENT:
        state = "SYN SENT";
        break;
    case RDP_SYN_RCVD:
        state = "SYN RCVD";
        break;
    case RDP_OPEN:
        state = "OPEN";
        break;
    case RDP_ACTIVE_CLOSE_WAIT:
        state = "ACTIVE CLOSE WAIT";
        break;
    case RDP_PASSIVE_CLOSE_WAIT:
        state = "PASSIVE CLOSE WAIT";
        break;
    }
    return Py_BuildValue("s", state);
}

static PyObject* py_rdp_listen(PyObject* self, PyObject* args)
{
    PyObject *connection;
    int port;
    if (!PyArg_ParseTuple(args, "Oi", &connection, &port))
        return NULL;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn == NULL)
        return NULL;
    if (port < 0 || port > 255)
    {
        return PyBool_FromLong(0);
    }
    bool res = rdp_listen(&conn->connection, port);
    return PyBool_FromLong(res);
}

static PyObject* py_rdp_connect(PyObject* self, PyObject* args)
{
    PyObject *connection;
    int local, remote;
    if (!PyArg_ParseTuple(args, "Oii", &connection, &local, &remote))
        return NULL;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn == NULL)
        return NULL;
    if (local < 0 || local > 255)
    {
        return PyBool_FromLong(0);
    }
    if (remote < 0 || remote > 255)
    {
        return PyBool_FromLong(0);
    }
    bool res = rdp_connect(&conn->connection, local, remote);
    return PyBool_FromLong(res);
}

static PyObject* py_rdp_close(PyObject* self, PyObject* args)
{
    PyObject *connection;
    if (!PyArg_ParseTuple(args, "O", &connection))
        return NULL;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn == NULL)
        return NULL;
    bool res = rdp_close(&conn->connection);
    return PyBool_FromLong(res);
}

static PyObject* py_rdp_reset(PyObject* self, PyObject* args)
{
    PyObject *connection;
    if (!PyArg_ParseTuple(args, "O", &connection))
        return NULL;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn == NULL)
        return NULL;
    rdp_reset_connection(&conn->connection);
    Py_RETURN_NONE;
}

static PyObject* py_rdp_send(PyObject* self, PyObject* args)
{
    PyObject *connection;
    const char *buf;
    int len;
    if (!PyArg_ParseTuple(args, "Oy#", &connection, &buf, &len))
        return NULL;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn == NULL)
        return NULL;
    if (len < 0)
        return NULL;
    if (buf == NULL)
        return NULL;
    bool res = rdp_send(&conn->connection, (const uint8_t*)buf, (size_t)len);
    return PyBool_FromLong(res);
}

static PyObject* py_rdp_dgram_receive(PyObject* self, PyObject* args)
{
    PyObject *connection;
    const char *buf;
    int len;
    if (!PyArg_ParseTuple(args, "Oy#", &connection, &buf, &len))
        return NULL;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn == NULL)
        return NULL;
    if (len < 0)
        return NULL;
    if (buf == NULL)
        return NULL;
    bool res = rdp_received(&conn->connection, (const uint8_t*)buf, (size_t)len);
    return PyBool_FromLong(res);
}


static PyObject* py_rdp_set_connected_cb(PyObject* self, PyObject* args)
{
    PyObject *connection;
    PyObject *cb;
    if (!PyArg_ParseTuple(args, "OO", &connection, &cb))
        return NULL;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn == NULL)
        return NULL;
    SET_CB(conn, connected_cb, cb);
    Py_RETURN_NONE;
}

static PyObject* py_rdp_set_closed_cb(PyObject* self, PyObject* args)
{
    PyObject *connection;
    PyObject *cb;
    if (!PyArg_ParseTuple(args, "OO", &connection, &cb))
        return NULL;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn == NULL)
        return NULL;
    SET_CB(conn, closed_cb, cb);
    Py_RETURN_NONE;
}

static PyObject* py_rdp_set_data_transmitted_cb(PyObject* self, PyObject* args)
{
    PyObject *connection;
    PyObject *cb;
    if (!PyArg_ParseTuple(args, "OO", &connection, &cb))
        return NULL;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn == NULL)
        return NULL;
    SET_CB(conn, data_transmitted_cb, cb);
    Py_RETURN_NONE;
}

static PyObject* py_rdp_set_data_received_cb(PyObject* self, PyObject* args)
{
    PyObject *connection;
    PyObject *cb;
    if (!PyArg_ParseTuple(args, "OO", &connection, &cb))
        return NULL;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn == NULL)
        return NULL;
    SET_CB(conn, data_received_cb, cb);
    Py_RETURN_NONE;
}

static PyObject* py_rdp_set_dgram_send_cb(PyObject* self, PyObject* args)
{
    PyObject *connection;
    PyObject *cb;
    if (!PyArg_ParseTuple(args, "OO", &connection, &cb))
        return NULL;
    struct py_rdp_connection_s *conn = PyCapsule_GetPointer(connection, "RDP Connection");
    if (conn == NULL)
        return NULL;
    SET_CB(conn, dgram_send_cb, cb);
    Py_RETURN_NONE;
}

static PyMethodDef myMethods[] = {
    { "create_connection", py_rdp_create_connection, METH_NOARGS, "Create connection" },
    { "state",  py_rdp_state, METH_VARARGS, "Connection state" },
    { "listen", py_rdp_listen, METH_VARARGS, "Listen port" },
    { "connect", py_rdp_connect, METH_VARARGS, "Connect to remote host" },
    { "close",  py_rdp_close, METH_VARARGS, "Close connection" },
    { "reset",  py_rdp_reset, METH_VARARGS, "Reset connection" },
    { "send", py_rdp_send, METH_VARARGS, "Send data"},
    { "datagram_receive", py_rdp_dgram_receive, METH_VARARGS, "Handle received datagram" },
    
    { "set_connected_cb", py_rdp_set_connected_cb, METH_VARARGS, "Set connected callback" },
    { "set_closed_cb", py_rdp_set_closed_cb, METH_VARARGS, "Set closed callback" },
    { "set_data_received_cb", py_rdp_set_data_received_cb, METH_VARARGS, "Set data_received callback" },
    { "set_data_transmitted_cb", py_rdp_set_data_transmitted_cb, METH_VARARGS, "Set data_transmitted callback" },
    { "set_dgram_send_cb", py_rdp_set_dgram_send_cb, METH_VARARGS, "Set dgram_send callback" },
    { NULL, NULL, 0, NULL }
};

static struct PyModuleDef myModule = {
    PyModuleDef_HEAD_INIT,
    "rdp.wrapper",
    "RDP communication module",
    -1,
    myMethods
};

PyMODINIT_FUNC PyInit_wrapper(void)
{
    return PyModule_Create(&myModule);
}
