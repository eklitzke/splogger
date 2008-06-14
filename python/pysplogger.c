/* Python splogger client
 *
 * Copyright Evan Klitzke, 2008
 */

#include <Python.h>
#include <sp.h>

typedef struct
{
	PyObject_HEAD
	mailbox mbox;
	service service_type;
	char pgroupname[MAX_GROUP_NAME];
} SploggerObject;

static PyObject*
splogger_broadcast(PyObject *self, PyObject *args);

static PyTypeObject splogger_SploggerType;

static PyObject*
splogger_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	const unsigned short int port;
	const char *host; /* XXX: remember to free this laster with PyMem_Free */
	if (!PyArg_ParseTuple(args, "Hs", &port, &host))
		return NULL;

	char port_s[6];
	if (sprintf(port_s, "%hu", port) < 0) {
		return NULL;
	}

	SploggerObject *obj = PyObject_NewVar(SploggerObject, &splogger_SploggerType, sizeof(SploggerObject *));

	/* Connect on 127.0.0.1:4803 */
	int ret = SP_connect(port_s, NULL, 0, 0, &(obj->mbox), obj->pgroupname);
	if (ret != ACCEPT_SESSION) {
		SP_error(ret);
		return NULL;
	}

	/* FIXME: add a port/hostname attribute */
	/* FIXME: reference counting? */
	return obj;
}

static PyObject *
splogger_broadcast(PyObject *pyself, PyObject *args)
{
	SploggerObject *self = (SploggerObject *) pyself;

	const int code, message_len;
	const char *message, *group;

	if (!PyArg_ParseTuple(args, "dss#", &code, &group, &message, &message_len))
		return NULL;

	/* Send out the message */
	int ret = SP_multicast(self->mbox, self->service_type, group, code,
			message_len, message);
	if (ret != message_len) {
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
		"broadcast", splogger_broadcast, METH_VARARGS,
		"broadcast(code, group, message) -- brodcast a message"
	},

	{NULL}
};

static PyTypeObject splogger_SploggerType = {
	PyObject_HEAD_INIT(NULL)
	0,							/* ob_size */
	"pysplog.Splogger",			/* tp_name */
	sizeof(SploggerObject),		/* tp_basicsize */
	0,							/* tp_itemsize */
	(destructor)splogger_dealloc,	/* tp_dealloc */
	0,							/* tp_print */
	0,							/* tp_getattr */
	0,							/* tp_setattr */
	0,							/* tp_compare */
	0,							/* tp_repr */
	0,							/* tp_as_number */
	0,							/* tp_as_sequence */
	0,							/* tp_as_mapping */
	0,							/* tp_hash */
	0,							/* tp_call */
	0,							/* tp_str */
	0,							/* tp_getattro */
	0,							/* tp_setattro */
	0,							/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/*tp_flags*/
	"Splogger client object",	/* tp_doc */
	0,							/* tp_traverse */
	0,							/* tp_clear */
	0,							/* tp_richcompare */
	0,							/* tp_weaklistoffset */
	0,							/* tp_iter */
	0,							/* tp_iternext */
	splogger_methods,			/* tp_methods */
	0,							/* tp_members */
	0,							/* tp_getset */
	0,							/* tp_base */
	0,							/* tp_dict */
	0,							/* tp_descr_get */
	0,							/* tp_descr_set */
	0,							/* tp_dictoffset */
	0,							/* tp_init */
	0,							/* tp_alloc */
	splogger_new,				/* tp_new */
};

static PyMethodDef splogger_module_methods[] = {
	{NULL}  /* Sentinel */
};

PyMODINIT_FUNC initsplogger(void)
{
	PyObject* m;

	splogger_SploggerType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&splogger_SploggerType) < 0)
		return;

	m = Py_InitModule3("splogger", splogger_module_methods, "Easy to use library for broadcasting messages with splogger.");

	Py_INCREF(&splogger_SploggerType);
	PyModule_AddObject(m, "Splogger", (PyObject *)&splogger_SploggerType);
}
