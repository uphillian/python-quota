/*
 * Copyright (c) 1980, 1990 Regents of the University of California. All
 * rights reserved.
 * 
 * This code is derived from software contributed to Berkeley by Robert Elz at
 * The University of Melbourne.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution. 3. All advertising
 * materials mentioning features or use of this software must display the
 * following acknowledgement: This product includes software developed by the
 * University of California, Berkeley and its contributors. 4. Neither the
 * name of the University nor the names of its contributors may be used to
 * endorse or promote products derived from this software without specific
 * prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* quotamodule.c
 * attempt to access quota information from python
 * Fri Apr 1 2011 Thomas Uphill <uphill@ias.edu>
 */

#ident "$Copyright: (c) 1980, 1990 Regents of the University of California. $"
#ident "$Copyright: All rights reserved. $"


#include <Python.h>
#include "structmember.h"
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

/* quota-tools Includes */
#include <sys/types.h>
#include <sys/param.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#ifdef RPC
#include <rpc/rpc.h>
#include "rquota.h"
#endif

#include "quota.h"
#include "quotaops.h"
#include "quotasys.h"
#include "pot.h"
#include "common.h"

#define FL_QUIET 1
#define FL_VERBOSE 2
#define FL_USER 4
#define FL_GROUP 8
#define FL_SMARTSIZE 16
#define FL_LOCALONLY 32
#define FL_QUIETREFUSE 64
#define FL_NOAUTOFS 128
#define FL_NOWRAP 256
#define FL_FSLIST 512
#define FL_NUMNAMES 1024
#define FL_NFSALL 2048
#define FL_RAWGRACE 4096
#define FL_NO_MIXED_PATHS 8192
/* quota-tools Includes */
#include <math.h>

/* quota-tools defines */
#define COMPILE_OPTS none
#define QUOTA_VERSION 0.1
/* quota-tools defines */

/* quota-tools globals */
int flags, fmt = -1;
char* progname = 'quota';
/* quota-tools globals */

typedef struct {
    PyObject_HEAD
    int uid;
    PyObject *username; 
    PyObject *filesystems; 
} Quota;

static void
Quota_dealloc(Quota* self)
{
    Py_XDECREF(self->username);
    self->ob_type->tp_free((PyObject*)self);
};

static PyObject *
Quota_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Quota *self;

    self = (Quota *)type->tp_alloc(type, 0);
    if (self != NULL) {
	self->uid = 0;
        self->username = PyString_FromString("");
        if (self->username == NULL)
          {
            Py_DECREF(self);
            return NULL;
          }
        
	self->filesystems = PyDict_New();
	if (self->filesystems == NULL)
	{
		Py_DECREF(self);
		return NULL;
	}
    }

    return (PyObject *)self;
};

PyObject *usageDict(long usage, long quota, long limit, long grace) 
{
	PyObject *q;
	q = PyDict_New();
	PyDict_SetItem(q,PyString_FromString("usage"),PyInt_FromLong(usage));
	PyDict_SetItem(q,PyString_FromString("quota"),PyInt_FromLong(quota));
	PyDict_SetItem(q,PyString_FromString("limit"),PyInt_FromLong(limit));
	PyDict_SetItem(q,PyString_FromString("grace"),PyInt_FromLong(grace));
	if (quota != 0) PyDict_SetItem(q,PyString_FromString("percentage"),PyInt_FromLong(round((usage/quota)*100)));
	else PyDict_SetItem(q,PyString_FromString("percentage"),PyInt_FromLong(-1));

	return q;
}

static int
Quota_init(Quota *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"uid","username", "filesystems", NULL};

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "|i", kwlist, 
                                      &self->uid))
        return -1; 

    if (!self->uid) {
	self->uid=getuid();
    }
    struct passwd *pw;
    if (( pw = getpwuid(self->uid) )) {
	self->username = PyString_FromString(pw->pw_name);
    }
// showquotas(USRQUOTA, getuid(), argc, argv)
	// filesystems
	struct dquot *qlist, *q;
        char *msgi, *msgb;
        char timebuf[MAXTIMELEN];
        char name[MAXNAMELEN];
        struct quota_handle **handles;
        int lines = 0, bover, iover, over;
        time_t now;
	int mntcnt = 0;
	char **mnt;

	time(&now);
	handles = create_handle_list(mntcnt, mnt, USRQUOTA, fmt,
                IOI_READONLY | ((flags & FL_NO_MIXED_PATHS) ? 0 : IOI_NFS_MIXED_PATHS),
                ((flags & FL_NOAUTOFS) ? MS_NO_AUTOFS : 0)
                | ((flags & FL_LOCALONLY) ? MS_LOCALONLY : 0)
                | ((flags & FL_NFSALL) ? MS_NFS_ALL : 0));
        qlist = getprivs(self->uid, handles, !!(flags & FL_QUIETREFUSE));
	over = 0;
	PyObject *usage;
	PyObject *qDict;
	for (q = qlist; q; q = q->dq_next) {
		// create a dictionary of the usage on this filesystem
		qDict = PyDict_New();
		usage = usageDict(q->dq_dqb.dqb_curspace/1024,q->dq_dqb.dqb_bhardlimit, q->dq_dqb.dqb_bsoftlimit, q->dq_dqb.dqb_btime);
		PyDict_SetItem(qDict,PyString_FromString("blocks"),usage);
		usage = usageDict(q->dq_dqb.dqb_curinodes,q->dq_dqb.dqb_ihardlimit,q->dq_dqb.dqb_isoftlimit,q->dq_dqb.dqb_itime);
		PyDict_SetItem(qDict,PyString_FromString("files"),usage);
		
		// add the blocks
		PyDict_SetItem(self->filesystems,PyString_FromString(q->dq_h->qh_quotadev),qDict);
		over++;
	}

/*	PyObject *blocks;
	blocks = PyDict_New();
	PyDict_SetItem(blocks,PyString_FromString("usage"),PyInt_FromLong(over));
	PyDict_SetItem(blocks,PyString_FromString("quota"),PyInt_FromLong(10));
	PyDict_SetItem(self->filesystems,PyString_FromString("scratch"),blocks);
*/

	return 0;
}


static PyMemberDef Quota_members[] = {
    {"uid", T_INT, offsetof(Quota,uid), 0,
     "uid of user"},
    {"username", T_OBJECT_EX, offsetof(Quota, username), 0,
     "username"},
    {"filesystems", T_OBJECT_EX, offsetof(Quota, filesystems), 0,
     "filesystems usage"},
    {NULL}  /* Sentinel */
};

static PyGetSetDef Quota_getseters[] = {
    {"uid", NULL, NULL, "uid of the user", NULL},
    {"username", NULL, NULL, "username", NULL},
    {"filesystems", NULL, NULL, "filesystem usage", NULL},
    {NULL}  /* Sentinel */
};


static PyTypeObject QuotaType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "noddy.Quota",             /*tp_name*/
    sizeof(Quota),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)Quota_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Quota objects",           /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    0,             /* tp_methods */
    Quota_members,             /* tp_members */
    Quota_getseters,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Quota_init,      /* tp_init */
    0,                         /* tp_alloc */
    Quota_new,                 /* tp_new */
};

static PyMethodDef module_methods[] = {
    {NULL}  /* Sentinel */
};

#ifndef PyMODINIT_FUNC	/* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initquota(void) 
{
    PyObject* m;

    if (PyType_Ready(&QuotaType) < 0)
        return;

    m = Py_InitModule3("quota", module_methods,
                       "Quota module that creates an extension type.");
    if (m == NULL)
      return;

    Py_INCREF(&QuotaType);
    PyModule_AddObject(m, "Quota", (PyObject *)&QuotaType);
}
