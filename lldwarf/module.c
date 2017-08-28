#include "lldwarf.h"

int read_uleb128(Py_buffer *buffer, Py_ssize_t *offset, uint64_t *ret)
{
	int shift = 0;
	uint8_t byte;

	*ret = 0;
	for (;;) {
		if (read_u8(buffer, offset, &byte) == -1)
			return -1;
		if (shift == 63 && byte > 1) {
			PyErr_SetString(PyExc_OverflowError,
					"ULEB128 overflowed unsigned 64-bit integer");
			return -1;
		}
		*ret |= (byte & UINT64_C(0x7f)) << shift;
		shift += 7;
		if (!(byte & 0x80))
			break;
	}
	return 0;
}

int read_sleb128(Py_buffer *buffer, Py_ssize_t *offset, int64_t *ret)
{
	int shift = 0;
	uint8_t byte;

	*ret = 0;
	for (;;) {
		if (read_u8(buffer, offset, &byte) == -1)
			return -1;
		if (shift == 63 && byte != 0 && byte != 0x7f) {
			PyErr_SetString(PyExc_OverflowError,
					"SLEB128 overflowed signed 64-bit integer");
			return -1;
		}
		*ret |= (byte & INT64_C(0x7f)) << shift;
		shift += 7;
		if (!(byte & 0x80))
			break;
	}

	if (shift < 64 && (byte & 0x40))
		*ret |= -(INT64_C(1) << shift);
	return 0;
}

int read_strlen(Py_buffer *buffer, Py_ssize_t *offset, Py_ssize_t *len)
{
	char *p, *nul;

	if (*offset >= buffer->len) {
		PyErr_Format(PyExc_ValueError,
			     "unexpected EOF while parsing string");
		return -1;
	}

	p = (char *)buffer->buf + *offset;
	nul = memchr(p, 0, buffer->len - *offset);
	if (!nul) {
		PyErr_Format(PyExc_ValueError, "unterminated string");
		return -1;
	}

	*len = nul - p;
	*offset += *len + 1;
	return 0;
}

static PyObject *parse_uleb128(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = {"buffer", "offset", NULL};
	Py_buffer buffer;
	Py_ssize_t offset = 0;
	uint64_t value;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "y*|n:parse_uleb128",
					 keywords, &buffer, &offset))
		return NULL;

	if (offset < 0) {
		PyErr_SetString(PyExc_ValueError, "offset cannot be negative");
		PyBuffer_Release(&buffer);
		return NULL;
	}

	if (read_uleb128(&buffer, &offset, &value) == -1) {
		if (PyErr_ExceptionMatches(PyExc_EOFError))
			PyErr_SetString(PyExc_ValueError, "ULEB128 is truncated");
		PyBuffer_Release(&buffer);
		return NULL;
	}

	PyBuffer_Release(&buffer);
	return PyLong_FromUnsignedLongLong(value);
}

static PyObject *parse_sleb128(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = {"buffer", "offset", NULL};
	Py_buffer buffer;
	Py_ssize_t offset = 0;
	int64_t value;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "y*|n:parse_sleb128",
					 keywords, &buffer, &offset))
		return NULL;

	if (offset < 0) {
		PyErr_SetString(PyExc_ValueError, "offset cannot be negative");
		PyBuffer_Release(&buffer);
		return NULL;
	}

	if (read_sleb128(&buffer, &offset, &value) == -1) {
		if (PyErr_ExceptionMatches(PyExc_EOFError))
			PyErr_SetString(PyExc_ValueError, "SLEB128 is truncated");
		PyBuffer_Release(&buffer);
		return NULL;
	}

	PyBuffer_Release(&buffer);
	return PyLong_FromLongLong(value);
}

static PyObject *parse_uleb128_offset(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = {"buffer", "offset", NULL};
	Py_buffer buffer;
	Py_ssize_t offset = 0;
	uint64_t value;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "y*|n:parse_uleb128_offset",
					 keywords, &buffer, &offset))
		return NULL;

	if (offset < 0) {
		PyErr_SetString(PyExc_ValueError, "offset cannot be negative");
		PyBuffer_Release(&buffer);
		return NULL;
	}

	if (read_uleb128(&buffer, &offset, &value) == -1) {
		if (PyErr_ExceptionMatches(PyExc_EOFError))
			PyErr_SetString(PyExc_ValueError, "ULEB128 is truncated");
		PyBuffer_Release(&buffer);
		return NULL;
	}

	PyBuffer_Release(&buffer);
	return Py_BuildValue("Kn", value, offset);
}

static PyObject *parse_abbrev_table(PyObject *self, PyObject *args,
				    PyObject *kwds)
{
	static char *keywords[] = {"buffer", "offset", NULL};
	Py_buffer buffer;
	Py_ssize_t offset = 0;
	PyObject *ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "y*|n:parse_abbrev_table",
					 keywords, &buffer, &offset))
		return NULL;

	if (offset < 0) {
		PyErr_SetString(PyExc_ValueError, "offset cannot be negative");
		PyBuffer_Release(&buffer);
		return NULL;
	}

	ret = LLDwarf_ParseAbbrevTable(&buffer, &offset);
	PyBuffer_Release(&buffer);
	return ret;
}

static PyObject *parse_arange_table(PyObject *self, PyObject *args,
				    PyObject *kwds)
{
	static char *keywords[] = {
		"segment_size", "address_size", "buffer", "offset", NULL
	};
	Py_ssize_t segment_size;
	Py_ssize_t address_size;
	Py_buffer buffer;
	Py_ssize_t offset = 0;
	PyObject *ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "nny*|n:parse_arange_table",
					 keywords, &segment_size, &address_size,
					 &buffer, &offset))
		return NULL;

	if (offset < 0) {
		PyErr_SetString(PyExc_ValueError, "offset cannot be negative");
		PyBuffer_Release(&buffer);
		return NULL;
	}

	ret = LLDwarf_ParseArangeTable(&buffer, &offset, segment_size, address_size);
	PyBuffer_Release(&buffer);
	return ret;
}

static PyObject *parse_arange_table_header(PyObject *self, PyObject *args,
					   PyObject *kwds)
{
	static char *keywords[] = {"buffer", "offset", NULL};
	Py_buffer buffer;
	Py_ssize_t offset = 0;
	PyObject *ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "y*|n:parse_arange_table_header",
					 keywords, &buffer, &offset))
		return NULL;

	if (offset < 0) {
		PyErr_SetString(PyExc_ValueError, "offset cannot be negative");
		PyBuffer_Release(&buffer);
		return NULL;
	}

	ret = LLDwarf_ParseArangeTableHeader(&buffer, &offset);
	PyBuffer_Release(&buffer);
	return ret;
}

static PyObject *parse_compilation_unit_header(PyObject *self, PyObject *args,
					       PyObject *kwds)
{
	static char *keywords[] = {"buffer", "offset", NULL};
	Py_buffer buffer;
	Py_ssize_t offset = 0;
	PyObject *ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwds,
					 "y*|n:parse_compilation_unit_header",
					 keywords, &buffer, &offset))
		return NULL;

	if (offset < 0) {
		PyErr_SetString(PyExc_ValueError, "offset cannot be negative");
		PyBuffer_Release(&buffer);
		return NULL;
	}

	ret = LLDwarf_ParseCompilationUnitHeader(&buffer, &offset);
	PyBuffer_Release(&buffer);
	return ret;
}

static PyObject *parse_die(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = {
		"cu", "parent", "abbrev_table", "cu_offset", "buffer", "offset",
		"recurse", NULL,
	};
	PyObject *cu, *parent, *abbrev_table;
	Py_ssize_t cu_offset;
	Py_buffer buffer;
	Py_ssize_t offset = 0;
	int recurse = 0;
	PyObject *ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!OO!ny*|np:parse_die",
					 keywords,
					 (PyObject *)&CompilationUnitHeader_type, &cu,
					 &parent, (PyObject *)&PyDict_Type,
					 &abbrev_table, &cu_offset, &buffer,
					 &offset, &recurse))
		return NULL;

	if (offset < 0) {
		PyErr_SetString(PyExc_ValueError, "offset cannot be negative");
		PyBuffer_Release(&buffer);
		return NULL;
	}

	ret = LLDwarf_ParseDie(&buffer, &offset, (CompilationUnitHeader *)cu,
			       parent, abbrev_table, cu_offset, recurse, false);
	if (!ret && !PyErr_Occurred()) {
		Py_INCREF(Py_None);
		ret = Py_None;
	}
	PyBuffer_Release(&buffer);
	return ret;
}

static PyObject *parse_die_siblings(PyObject *self, PyObject *args,
				    PyObject *kwds)
{
	static char *keywords[] = {
		"cu", "parent", "abbrev_table", "cu_offset", "buffer", "offset",
		"recurse", NULL,
	};
	PyObject *cu, *parent, *abbrev_table;
	Py_ssize_t cu_offset;
	Py_buffer buffer;
	Py_ssize_t offset = 0;
	int recurse = 0;
	PyObject *ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!OO!ny*|np:parse_die_siblings",
					 keywords,
					 (PyObject *)&CompilationUnitHeader_type,
					 &cu, &parent, (PyObject *)&PyDict_Type,
					 &abbrev_table, &cu_offset, &buffer,
					 &offset, &recurse))
		return NULL;

	if (offset < 0) {
		PyErr_SetString(PyExc_ValueError, "offset cannot be negative");
		PyBuffer_Release(&buffer);
		return NULL;
	}

	ret = LLDwarf_ParseDieSiblings(&buffer, &offset, (CompilationUnitHeader *)cu,
				       parent, abbrev_table, cu_offset, recurse);
	PyBuffer_Release(&buffer);
	return ret;
}

static PyObject *parse_line_number_program_header(PyObject *self,
						  PyObject *args,
						  PyObject *kwds)
{
	static char *keywords[] = {"buffer", "offset", NULL};
	Py_buffer buffer;
	Py_ssize_t offset = 0;
	PyObject *ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwds,
					 "y*|n:parse_line_number_program_header",
					 keywords, &buffer, &offset))
		return NULL;

	if (offset < 0) {
		PyErr_SetString(PyExc_ValueError, "offset cannot be negative");
		PyBuffer_Release(&buffer);
		return NULL;
	}

	ret = LLDwarf_ParseLineNumberProgramHeader(&buffer, &offset);
	PyBuffer_Release(&buffer);
	return ret;
}

static PyObject *execute_line_number_program(PyObject *self, PyObject *args,
					     PyObject *kwds)
{
	static char *keywords[] = {
		"lnp", "lnp_end_offset", "buffer", "offset", NULL
	};
	PyObject *lnp;
	Py_ssize_t lnp_end_offset;
	Py_buffer buffer;
	Py_ssize_t offset = 0;
	PyObject *ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwds,
					 "O!ny*|n:execute_line_number_program",
					 keywords,
					 (PyObject *)&LineNumberProgramHeader_type,
					 &lnp, &lnp_end_offset, &buffer,
					 &offset))
		return NULL;

	if (offset < 0) {
		PyErr_SetString(PyExc_ValueError, "offset cannot be negative");
		PyBuffer_Release(&buffer);
		return NULL;
	}

	ret = LLDwarf_ExecuteLineNumberProgram(&buffer, &offset,
					       (LineNumberProgramHeader *)lnp,
					       lnp_end_offset);
	PyBuffer_Release(&buffer);
	return ret;
}

static PyObject *parse_range_list(PyObject *self, PyObject *args,
				  PyObject *kwds)
{
	static char *keywords[] = {
		"address_size", "buffer", "offset", NULL
	};
	Py_ssize_t address_size;
	Py_buffer buffer;
	Py_ssize_t offset = 0;
	PyObject *ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwds,
					 "ny*|n:execute_line_number_program",
					 keywords, &address_size, &buffer,
					 &offset))
		return NULL;

	if (offset < 0) {
		PyErr_SetString(PyExc_ValueError, "offset cannot be negative");
		PyBuffer_Release(&buffer);
		return NULL;
	}

	ret = LLDwarf_ParseRangeList(&buffer, &offset, address_size);
	PyBuffer_Release(&buffer);
	return ret;
}

static PyMethodDef lldwarf_methods[] = {
	{"parse_uleb128", (PyCFunction)parse_uleb128,
	 METH_VARARGS | METH_KEYWORDS,
	 "parse_uleb128(buffer, offset=0) -> int\n\n"
	 "Parse an unsigned LEB128-encoded integer.\n\n"
	 "Arguments:\n"
	 "buffer -- readable source buffer\n"
	 "offset -- optional offset into the buffer"},
	{"parse_sleb128", (PyCFunction)parse_sleb128,
	 METH_VARARGS | METH_KEYWORDS,
	 "parse_sleb128(buffer, offset=0) -> int\n\n"
	 "Parse a signed LEB128-encoded integer.\n\n"
	 "Arguments:\n"
	 "buffer -- readable source buffer\n"
	 "offset -- optional offset into the buffer"},
	{"parse_uleb128_offset", (PyCFunction)parse_uleb128_offset,
	 METH_VARARGS | METH_KEYWORDS,
	 "parse_uleb128_offset(buffer, offset=0) -> (int, int)\n\n"
	 "Like parse_uleb128() but also returns the ending offset in the\n"
	 "buffer.\n\n"
	 "Arguments:\n"
	 "buffer -- readable source buffer\n"
	 "offset -- optional offset into the buffer"},
	{"parse_abbrev_table", (PyCFunction)parse_abbrev_table,
	 METH_VARARGS | METH_KEYWORDS,
	 "parse_abbrev_table(buffer, offset=0) -> dict[code]: AbbrevDecl\n\n"
	 "Parse an abbreviation table.\n\n"
	 "Arguments:\n"
	 "buffer -- readable source buffer\n"
	 "offset -- optional offset into the buffer"},
	{"parse_arange_table", (PyCFunction)parse_arange_table,
	 METH_VARARGS | METH_KEYWORDS,
	 "parse_arange_table(segment_size, address_size, buffer, offset=0) -> list of AddressRange\n\n"
	 "Parse an address range table.\n\n"
	 "Arguments:\n"
	 "segment_size -- size of a segment selector in this arange table\n"
	 "address_size -- size of an address in this arange table\n"
	 "buffer -- readable source buffer\n"
	 "offset -- optional offset into the buffer"},
	{"parse_arange_table_header", (PyCFunction)parse_arange_table_header,
	 METH_VARARGS | METH_KEYWORDS,
	 "parse_arange_table_header(buffer, offset=0) -> dict[code]: ArangeTableHeader\n\n"
	 "Parse an address range table header.\n\n"
	 "Arguments:\n"
	 "buffer -- readable source buffer\n"
	 "offset -- optional offset into the buffer"},
	{"parse_compilation_unit_header",
	 (PyCFunction)parse_compilation_unit_header,
	 METH_VARARGS | METH_KEYWORDS,
	 "parse_compilation_unit_header(buffer, offset=0) -> CompilationUnitHeader\n\n"
	 "Parse a compilation unit header.\n\n"
	 "Arguments:\n"
	 "buffer -- readable source buffer\n"
	 "offset -- optional offset into the buffer"},
	{"parse_die", (PyCFunction)parse_die, METH_VARARGS | METH_KEYWORDS,
	 "parse_die(cu, abbrev_table, cu_offset, buffer, offset=0, recurse=False) -> DwarfDie\n\n"
	 "Parse a debugging information entry.\n\n"
	 "Arguments:\n"
	 "cu -- compilation unit header\n"
	 "abbrev_table -- abbreviation table\n"
	 "cu_offset -- offset into the buffer where the CU header was parsed\n"
	 "buffer -- readable source buffer\n"
	 "offset -- optional offset into the buffer\n"
	 "recurse -- boolean specifying whether to also parse the DIE's children"},
	{"parse_die_siblings", (PyCFunction)parse_die_siblings,
	  METH_VARARGS | METH_KEYWORDS,
	 "parse_die_siblings(cu, abbrev_table, cu_offset, buffer, offset=0, recurse=False) -> DwarfDie\n\n"
	 "Parse a list of sibling debugging information entries.\n\n"
	 "Arguments:\n"
	 "cu -- compilation unit header\n"
	 "abbrev_table -- abbreviation table\n"
	 "cu_offset -- offset into the buffer where the CU header was parsed\n"
	 "buffer -- readable source buffer\n"
	 "offset -- optional offset into the buffer\n"
	 "recurse -- boolean specifying whether to also parse the DIEs' children"},
	{"parse_line_number_program_header",
	 (PyCFunction)parse_line_number_program_header,
	 METH_VARARGS | METH_KEYWORDS,
	 "parse_line_number_program_header(buffer, offset=0) -> LineNumberProgramHeader\n\n"
	 "Parse a line number program header.\n\n"
	 "Arguments:\n"
	 "buffer -- readable source buffer\n"
	 "offset -- optional offset into the buffer"},
	{"execute_line_number_program",
	 (PyCFunction)execute_line_number_program,
	 METH_VARARGS | METH_KEYWORDS,
	 "execute_line_number_program(lnp, lnp_end_offset, buffer, offset=0) -> list of LineNumberRow\n\n"
	 "Execute a line number program to reconstruct the line number\n"
	 "information matrix.\n\n"
	 "Arguments:\n"
	 "lnp -- line number program header\n"
	 "lnp_end_offset -- offset into the buffer where the line number program ends\n"
	 "buffer -- readable source buffer\n"
	 "offset -- optional offset into the buffer"},
	{"parse_range_list",
	 (PyCFunction)parse_range_list,
	 METH_VARARGS | METH_KEYWORDS,
	 "parse_range_list(address_size, buffer, offset=0) -> list of Range\n\n"
	 "Parse a range list.\n\n"
	 "Arguments:\n"
	 "address_size -- size of an address in this range list\n"
	 "buffer -- readable source buffer\n"
	 "offset -- optional offset into the buffer"},
	{},
};

static struct PyModuleDef lldwarfmodule = {
	PyModuleDef_HEAD_INIT,
	"lldwarf",
	"Low-level DWARF debugging format library",
	-1,
	lldwarf_methods,
};

PyMODINIT_FUNC
PyInit_lldwarf(void)
{
	PyObject *m;

	empty_tuple = PyTuple_New(0);
	if (!empty_tuple)
		return NULL;

	if (PyType_Ready(&AbbrevDecl_type) < 0)
		return NULL;

	AddressRange_type.tp_new = PyType_GenericNew;
	if (PyType_Ready(&AddressRange_type) < 0)
		return NULL;

	ArangeTableHeader_type.tp_new = PyType_GenericNew;
	if (PyType_Ready(&ArangeTableHeader_type) < 0)
		return NULL;

	CompilationUnitHeader_type.tp_new = PyType_GenericNew;
	if (PyType_Ready(&CompilationUnitHeader_type) < 0)
		return NULL;

	if (PyType_Ready(&DwarfDie_type) < 0)
		return NULL;

	LineNumberProgramHeader_type.tp_new = PyType_GenericNew;
	if (PyType_Ready(&LineNumberProgramHeader_type) < 0)
		return NULL;

	LineNumberRow_type.tp_new = PyType_GenericNew;
	if (PyType_Ready(&LineNumberRow_type) < 0)
		return NULL;

	Range_type.tp_new = PyType_GenericNew;
	if (PyType_Ready(&Range_type) < 0)
		return NULL;

#ifdef TEST_LLDWARFOBJECT
	TestObject_type.tp_new = PyType_GenericNew;
	if (PyType_Ready(&TestObject_type) < 0)
		return NULL;
#endif

	m = PyModule_Create(&lldwarfmodule);
	if (m == NULL)
		return NULL;

	Py_INCREF(&AbbrevDecl_type);
	PyModule_AddObject(m, "AbbrevDecl", (PyObject *)&AbbrevDecl_type);

	Py_INCREF(&AddressRange_type);
	PyModule_AddObject(m, "AddressRange", (PyObject *)&AddressRange_type);

	Py_INCREF(&ArangeTableHeader_type);
	PyModule_AddObject(m, "ArangeTableHeader", (PyObject *)&ArangeTableHeader_type);

	Py_INCREF(&CompilationUnitHeader_type);
	PyModule_AddObject(m, "CompilationUnitHeader",
			   (PyObject *)&CompilationUnitHeader_type);

	Py_INCREF(&DwarfDie_type);
	PyModule_AddObject(m, "DwarfDie", (PyObject *)&DwarfDie_type);

	Py_INCREF(&LineNumberProgramHeader_type);
	PyModule_AddObject(m, "LineNumberProgramHeader",
			   (PyObject *)&LineNumberProgramHeader_type);

	Py_INCREF(&LineNumberRow_type);
	PyModule_AddObject(m, "LineNumberRow", (PyObject *)&LineNumberRow_type);

	Py_INCREF(&Range_type);
	PyModule_AddObject(m, "Range", (PyObject *)&Range_type);

#ifdef TEST_LLDWARFOBJECT
	Py_INCREF(&TestObject_type);
	PyModule_AddObject(m, "_TestObject", (PyObject *)&TestObject_type);
#endif

	return m;
}
