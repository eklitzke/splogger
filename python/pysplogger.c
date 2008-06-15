/* Python splogger client
 *
 * Copyright Evan Klitzke, 2008
 */

#include <Python.h>
#include <sp.h>

#ifndef SPLOGGER_DEFAULT_PORT
#define SPLOGGER_DEFAULT_PORT 4803
#endif

#ifndef SPLOGGER_DEFAULT_HOST
#define SPLOGGER_DEFAULT_HOST "localhost"
#endif

#define SPLOGGER_MIN_PORT 1
#define SPLOGGER_MAX_PORT 65535
#define SPLOGGER_PORT_STR "Port must be an integer in the range 1 <= port <= 65535"

typedef struct
{
	PyObject_HEAD
	mailbox mbox;
	service service_type;
	char pgroupname[MAX_GROUP_NAME];
} SploggerObject;

static PyObject*
splogger_broadcast(PyObject *self, PyObject *args, PyObject *kwds);

static PyTypeObject splogger_SploggerType;

static int
splogger_init(SploggerObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *port = NULL;
	PyObject *host = NULL;

	/* The default service type is RELIABLE_MESS (i.e. reliable transport
	 * delivered on top of UDP) */
	int service_type = RELIABLE_MESS;

	static char *kwlist[] = {"port", "host", "service_type", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O!Sd", kwlist, &PyInt_Type,
				&port, &host, &service_type))
		return -1;

	if (port == NULL)
		port = PyInt_FromLong(SPLOGGER_DEFAULT_PORT);

	if (host == NULL)
		host = PyString_FromString(SPLOGGER_DEFAULT_HOST);

	PyObject *min_port = PyLong_FromLong(SPLOGGER_MIN_PORT);
	PyObject *max_port = PyLong_FromLong(SPLOGGER_MAX_PORT);

#define PORT_CMP_FAIL\
	Py_DECREF(port);\
	Py_DECREF(host);\
	Py_DECREF(min_port);\
	Py_DECREF(max_port);\
	return -1

	/* Check if the port is too big or too small */
	int too_small, too_large;
	if (PyObject_Cmp(port, min_port, &too_small) == -1) {
		PORT_CMP_FAIL;
	} if (PyObject_Cmp(port, max_port, &too_large) == -1) {
		PORT_CMP_FAIL;
	}

	Py_DECREF(min_port);
	Py_DECREF(max_port);

	if ((too_small == -1) || (too_large == 1)) {
		PyErr_SetString(PyExc_ValueError, SPLOGGER_PORT_STR);
		return -1;
	}

	/* Convert the port into a string */
	PyObject *port_as_py_string = PyObject_Str(port);
	const char *port_s = PyString_AsString(port_as_py_string);
	const char *host_s = PyString_AsString(host);
	char connect_string[strlen(port_s) + strlen(host_s) + 2];
	sprintf(connect_string, "%s@%s", port_s, host_s);
	Py_DECREF(port_as_py_string);

	/* Connect on 127.0.0.1:4803 */
	int ret = SP_connect(connect_string, NULL, 0, 0, &(self->mbox), self->pgroupname);

	self->service_type = FIFO_MESS;

	/* FIXME */

	if (ret != ACCEPT_SESSION) {
		SP_error(ret);
		return -1;
	}

#if 0
	PyObject *pyself = (PyObject *) self;
	const char *p = "port";

	if (PyObject_SetAttrString(pyself, p, port) == -1) {
		Py_DECREF(port);
		Py_DECREF(host);
		return -1;
	}
	if (PyObject_SetAttrString(pyself, "host", host) == -1) {
		Py_DECREF(port);
		Py_DECREF(host);
		return -1;
	}
#else
	Py_DECREF(port);
	Py_DECREF(host);
#endif

	return 0;
}

static PyObject *
splogger_broadcast(PyObject *pyself, PyObject *args, PyObject *kwds)
{
	SploggerObject *self = (SploggerObject *) pyself;

	const short int code;
	const int message_len;
	const char *message, *group;
	int service_type = self->service_type;

	static char *kwlist[] = {"code", "group", "message", "service_type", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "hss#|d", kwlist, &code,
				&group, &message, &message_len, &service_type))
		return NULL;

	/* Send out the message */
	int ret = SP_multicast(self->mbox, service_type, group, (int16) code,
			message_len, message);
	if (ret != message_len) {
		SP_error(ret);
		return NULL; /* FIXME: raise the right kind of error */
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static void
splogger_dealloc(SploggerObject* self)
{
	SP_disconnect(self->mbox);
}

static PyMethodDef splogger_methods[] = {
	{
		"broadcast", (PyCFunction) splogger_broadcast, METH_KEYWORDS,
		"broadcast(code, group, message) -- brodcast a message"
	},

	{NULL}
};

static PyTypeObject splogger_SploggerType = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,								/* ob_size */
	"splogger.Splogger",			/* tp_name */
	sizeof(SploggerObject),			/* tp_basicsize */
	0,								/* tp_itemsize */
	(destructor)splogger_dealloc,	/* tp_dealloc */
	0,								/* tp_print */
	0,								/* tp_getattr */
	0,								/* tp_setattr */
	0,								/* tp_compare */
	0,								/* tp_repr */
	0,								/* tp_as_number */
	0,								/* tp_as_sequence */
	0,								/* tp_as_mapping */
	0,								/* tp_hash */
	0,								/* tp_call */
	0,								/* tp_str */
	PyObject_GenericGetAttr,		/* tp_getattro */
	PyObject_GenericSetAttr,		/* tp_setattro */
	0,								/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,				/* tp_flags*/ /* FIXME: cannot subtype */
"This class implements a client for the splogger service. Clients connect to a\n\
spread daemon upon instantiation. It is not possible to connect to another\n\
host. If you want to manually close the connection and remove the client from\n\
the spread swarm, you must delete the client (i.e. del splogger_client).", /* tp_doc */
	0,								/* tp_traverse */
	0,								/* tp_clear */
	0,								/* tp_richcompare */
	0,								/* tp_weaklistoffset */
	0,								/* tp_iter */
	0,								/* tp_iternext */
	splogger_methods,				/* tp_methods */
	0,								/* tp_members */
	0,								/* tp_getset */
	0,								/* tp_base */
	0,								/* tp_dict */
	0,								/* tp_descr_get */
	0,								/* tp_descr_set */
	0,								/* tp_dictoffset */
	(initproc)splogger_init,		/* tp_init */
	0,								/* tp_alloc */
	PyType_GenericNew,				/* tp_new */
};

static PyMethodDef splogger_module_methods[] = {
	{NULL}	/* Sentinel */
};

PyMODINIT_FUNC initsplogger(void)
{
	PyObject* m;

	if (PyType_Ready(&splogger_SploggerType) < 0)
		return;

	m = Py_InitModule3("splogger", splogger_module_methods, "Easy to use library for broadcasting messages with splogger.");

	Py_INCREF(&splogger_SploggerType);
	PyModule_AddObject(m, "Splogger", (PyObject *)&splogger_SploggerType);

	/* Now we add all of the service_type flags to the module */
	PyModule_AddIntConstant(m, "UNRELIABLE_MESS", UNRELIABLE_MESS);
	PyModule_AddIntConstant(m, "RELIABLE_MESS", RELIABLE_MESS);
	PyModule_AddIntConstant(m, "FIFO_MESS", FIFO_MESS);
	PyModule_AddIntConstant(m, "CAUSAL_MESS", CAUSAL_MESS);
	PyModule_AddIntConstant(m, "AGREED_MESS", AGREED_MESS);
	PyModule_AddIntConstant(m, "SAFE_MESS", SAFE_MESS);
}
