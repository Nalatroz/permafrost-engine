/*
 *  This file is part of Permafrost Engine. 
 *  Copyright (C) 2018-2020 Eduard Permyakov 
 *
 *  Permafrost Engine is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Permafrost Engine is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *  Linking this software statically or dynamically with other modules is making 
 *  a combined work based on this software. Thus, the terms and conditions of 
 *  the GNU General Public License cover the whole combination. 
 *  
 *  As a special exception, the copyright holders of Permafrost Engine give 
 *  you permission to link Permafrost Engine with independent modules to produce 
 *  an executable, regardless of the license terms of these independent 
 *  modules, and to copy and distribute the resulting executable under 
 *  terms of your choice, provided that you also meet, for each linked 
 *  independent module, the terms and conditions of the license of that 
 *  module. An independent module is a module which is not derived from 
 *  or based on Permafrost Engine. If you modify Permafrost Engine, you may 
 *  extend this exception to your version of Permafrost Engine, but you are not 
 *  obliged to do so. If you do not wish to do so, delete this exception 
 *  statement from your version.
 *
 */

#include <Python.h> /* must be first */
#include "py_entity.h" 
#include "py_pickle.h"
#include "../entity.h"
#include "../event.h"
#include "../asset_load.h"
#include "../anim/public/anim.h"
#include "../game/public/game.h"
#include "../lib/public/khash.h"
#include "../lib/public/SDL_vec_rwops.h"
#include "../lib/public/pf_string.h"

#include <assert.h>


#define CHK_TRUE(_pred, _label) do{ if(!(_pred)) goto _label; }while(0)

typedef struct {
    PyObject_HEAD
    struct entity *ent;
}PyEntityObject;

typedef struct {
    PyEntityObject super; 
}PyAnimEntityObject;

typedef struct {
    PyEntityObject super; 
}PyCombatableEntityObject;

static PyObject *PyEntity_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
static void      PyEntity_dealloc(PyEntityObject *self);
static PyObject *PyEntity_del(PyEntityObject *self);
static PyObject *PyEntity_get_name(PyEntityObject *self, void *closure);
static int       PyEntity_set_name(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_get_pos(PyEntityObject *self, void *closure);
static int       PyEntity_set_pos(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_get_scale(PyEntityObject *self, void *closure);
static int       PyEntity_set_scale(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_get_rotation(PyEntityObject *self, void *closure);
static int       PyEntity_set_rotation(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_get_selectable(PyEntityObject *self, void *closure);
static int       PyEntity_set_selectable(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_get_selection_radius(PyEntityObject *self, void *closure);
static int       PyEntity_set_selection_radius(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_get_pfobj_path(PyEntityObject *self, void *closure);
static PyObject *PyEntity_get_speed(PyEntityObject *self, void *closure);
static int       PyEntity_set_speed(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_get_faction_id(PyEntityObject *self, void *closure);
static int       PyEntity_set_faction_id(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_get_vision_range(PyEntityObject *self, void *closure);
static int       PyEntity_set_vision_range(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_register(PyEntityObject *self, PyObject *args);
static PyObject *PyEntity_unregister(PyEntityObject *self, PyObject *args);
static PyObject *PyEntity_notify(PyEntityObject *self, PyObject *args);
static PyObject *PyEntity_select(PyEntityObject *self);
static PyObject *PyEntity_deselect(PyEntityObject *self);
static PyObject *PyEntity_stop(PyEntityObject *self);
static PyObject *PyEntity_move(PyEntityObject *self, PyObject *args);
static PyObject *PyEntity_pickle(PyEntityObject *self);
static PyObject *PyEntity_unpickle(PyObject *cls, PyObject *args);

static int       PyAnimEntity_init(PyAnimEntityObject *self, PyObject *args, PyObject *kwds);
static PyObject *PyAnimEntity_del(PyAnimEntityObject *self);
static PyObject *PyAnimEntity_play_anim(PyAnimEntityObject *self, PyObject *args, PyObject *kwds);
static PyObject *PyAnimEntity_get_anim(PyAnimEntityObject *self);
static PyObject *PyAnimEntity_pickle(PyAnimEntityObject *self);
static PyObject *PyAnimEntity_unpickle(PyObject *cls, PyObject *args);

static int       PyCombatableEntity_init(PyCombatableEntityObject *self, PyObject *args, PyObject *kwds);
static PyObject *PyCombatableEntity_del(PyCombatableEntityObject *self);
static PyObject *PyCombatableEntity_get_max_hp(PyCombatableEntityObject *self, void *closure);
static int       PyCombatableEntity_set_max_hp(PyCombatableEntityObject *self, PyObject *value, void *closure);
static PyObject *PyCombatableEntity_get_base_dmg(PyCombatableEntityObject *self, void *closure);
static int       PyCombatableEntity_set_base_dmg(PyCombatableEntityObject *self, PyObject *value, void *closure);
static PyObject *PyCombatableEntity_get_base_armour(PyCombatableEntityObject *self, void *closure);
static int       PyCombatableEntity_set_base_armour(PyCombatableEntityObject *self, PyObject *value, void *closure);
static PyObject *PyCombatableEntity_hold_position(PyCombatableEntityObject *self);
static PyObject *PyCombatableEntity_attack(PyCombatableEntityObject *self, PyObject *args);
static PyObject *PyCombatableEntity_pickle(PyCombatableEntityObject *self);
static PyObject *PyCombatableEntity_unpickle(PyObject *cls, PyObject *args);

/*****************************************************************************/
/* STATIC VARIABLES                                                          */
/*****************************************************************************/

/* pf.Entity */

static PyMethodDef PyEntity_methods[] = {
    {"__del__", 
    (PyCFunction)PyEntity_del, METH_NOARGS,
    "Calls the next __del__ in the MRO if there is one, otherwise do nothing."},

    {"register", 
    (PyCFunction)PyEntity_register, METH_VARARGS,
    "Registers the specified callable to be invoked when an event of the specified type "
    "is sent to this entity." },

    {"unregister", 
    (PyCFunction)PyEntity_unregister, METH_VARARGS,
    "Unregisters a callable previously registered to be invoked on the specified event."},

    {"notify", 
    (PyCFunction)PyEntity_notify, METH_VARARGS,
    "Send a specific event to an entity in order to invoke the entity's event handlers. Weakref "
    "arguments are automatically unpacked before being passed to the handler."},

    {"select", 
    (PyCFunction)PyEntity_select, METH_NOARGS,
    "Adds the entity to the current unit selection, if it is not present there already."},

    {"deselect", 
    (PyCFunction)PyEntity_deselect, METH_NOARGS,
    "Removes the entity from the current unit selection, if it is selected."},

    {"stop", 
    (PyCFunction)PyEntity_stop, METH_NOARGS,
    "Issues a 'stop' command to the entity, stopping its' movement and attack. Cancels 'hold position' order."},

    {"move", 
    (PyCFunction)PyEntity_move, METH_VARARGS,
    "Issues a 'move' order to the entity at the XZ position specified by the argument."},

    {"__pickle__", 
    (PyCFunction)PyEntity_pickle, METH_NOARGS,
    "Serialize a Permafrost Engine entity to a string."},

    {"__unpickle__", 
    (PyCFunction)PyEntity_unpickle, METH_VARARGS | METH_CLASS,
    "Create a new pf.Entity instance from a string earlier returned from a __pickle__ method."
    "Returns a tuple of the new instance and the number of bytes consumed from the stream."},

    {NULL}  /* Sentinel */
};

static PyGetSetDef PyEntity_getset[] = {
    {"name",
    (getter)PyEntity_get_name, (setter)PyEntity_set_name,
    "Custom name given to this enity.",
    NULL},
    {"pos",
    (getter)PyEntity_get_pos, (setter)PyEntity_set_pos,
    "The XYZ position in worldspace coordinates.",
    NULL},
    {"scale",
    (getter)PyEntity_get_scale, (setter)PyEntity_set_scale,
    "The XYZ scaling factors.",
    NULL},
    {"rotation",
    (getter)PyEntity_get_rotation, (setter)PyEntity_set_rotation,
    "XYZW quaternion for rotaion about local origin.",
    NULL},
    {"selectable",
    (getter)PyEntity_get_selectable, (setter)PyEntity_set_selectable,
    "Flag indicating whether this entity can be selected with the mouse.",
    NULL},
    {"selection_radius",
    (getter)PyEntity_get_selection_radius, (setter)PyEntity_set_selection_radius,
    "Radius (in OpenGL coordinates) of the unit selection circle for this entity.",
    NULL},
    {"pfobj_path",
    (getter)PyEntity_get_pfobj_path, NULL,
    "The relative path of the PFOBJ file used to instantiate the entity. Readonly.",
    NULL},
    {"speed",
    (getter)PyEntity_get_speed, (setter)PyEntity_set_speed,
    "Entity's movement speed (in OpenGL coordinates per second).",
    NULL},
    {"faction_id",
    (getter)PyEntity_get_faction_id, (setter)PyEntity_set_faction_id,
    "Index of the faction that the entity belongs to.",
    NULL},
    {"vision_range",
    (getter)PyEntity_get_vision_range, (setter)PyEntity_set_vision_range,
    "The radius (in OpenGL coordinates) that the entity sees around itself.",
    NULL},
    {NULL}  /* Sentinel */
};

static PyTypeObject PyEntity_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "pf.Entity",               /* tp_name */
    sizeof(PyEntityObject),    /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)PyEntity_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_reserved */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash  */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    "Permafrost Engine generic game entity.", /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    PyEntity_methods,          /* tp_methods */
    0,                         /* tp_members */
    PyEntity_getset,           /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    PyEntity_new,              /* tp_new */
};

/* pf.AnimEntity */

static PyMethodDef PyAnimEntity_methods[] = {
    {"play_anim", 
    (PyCFunction)PyAnimEntity_play_anim, METH_VARARGS | METH_KEYWORDS,
    "Play the animation clip with the specified name. "
    "Set kwarg 'mode=\%d' to set the animation mode. The default is ANIM_MODE_LOOP."},

    {"get_anim", 
    (PyCFunction)PyAnimEntity_get_anim, METH_NOARGS,
    "Get the name of the currently playing animation clip."},

    {"__del__", 
    (PyCFunction)PyAnimEntity_del, METH_NOARGS,
    "Calls the next __del__ in the MRO if there is one, otherwise do nothing."},

    {"__pickle__", 
    (PyCFunction)PyAnimEntity_pickle, METH_NOARGS,
    "Serialize a Permafrost Engine animated entity to a string."},

    {"__unpickle__", 
    (PyCFunction)PyAnimEntity_unpickle, METH_VARARGS | METH_CLASS,
    "Create a new pf.Entity instance from a string earlier returned from a __pickle__ method."
    "Returns a tuple of the new instance and the number of bytes consumed from the stream."},

    {NULL}  /* Sentinel */
};

static PyTypeObject PyAnimEntity_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "pf.AnimEntity",
    .tp_basicsize = sizeof(PyAnimEntityObject), 
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "Permafrost Engine animated entity. This type requires the "
                    "'idle_clip' keyword argument to be passed to __init__. "
                    "This is a subclass of pf.Entity.",
    .tp_methods   = PyAnimEntity_methods,
    .tp_base      = &PyEntity_type,
    .tp_init      = (initproc)PyAnimEntity_init,
};

/* pf.CombatableEntity */

static PyMethodDef PyCombatableEntity_methods[] = {
    {"hold_position", 
    (PyCFunction)PyCombatableEntity_hold_position, METH_NOARGS,
    "Issues a 'hold position' order to the entity, stopping it and preventing it from moving to attack."},

    {"attack", 
    (PyCFunction)PyCombatableEntity_attack, METH_VARARGS,
    "Issues an 'attack move' order to the entity at the XZ position specified by the argument."},

    {"__del__", 
    (PyCFunction)PyCombatableEntity_del, METH_NOARGS,
    "Calls the next __del__ in the MRO if there is one, otherwise do nothing."},

    {"__pickle__", 
    (PyCFunction)PyCombatableEntity_pickle, METH_NOARGS,
    "Serialize a Permafrost Engine combatable entity to a string."},

    {"__unpickle__", 
    (PyCFunction)PyCombatableEntity_unpickle, METH_VARARGS | METH_CLASS,
    "Create a new pf.Entity instance from a string earlier returned from a __pickle__ method."
    "Returns a tuple of the new instance and the number of bytes consumed from the stream."},

    {NULL}  /* Sentinel */
};

static PyGetSetDef PyCombatableEntity_getset[] = {
    {"max_hp",
    (getter)PyCombatableEntity_get_max_hp, (setter)PyCombatableEntity_set_max_hp,
    "The maximum number of hitpoints that the entity starts out with.",
    NULL},
    {"base_dmg",
    (getter)PyCombatableEntity_get_base_dmg, (setter)PyCombatableEntity_set_base_dmg,
    "The base damage for which this entity's attacks hit.",
    NULL},
    {"base_armour",
    (getter)PyCombatableEntity_get_base_armour, (setter)PyCombatableEntity_set_base_armour,
    "The base armour (as a fraction from 0.0 to 1.0) specifying which percentage of incoming "
    "damage is blocked.",
    NULL},
    {NULL}  /* Sentinel */
};

static PyTypeObject PyCombatableEntity_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "pf.CombatableEntity",
    .tp_basicsize = sizeof(PyCombatableEntityObject), 
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "Permafrost Engine entity which is able to take part in combat. This type "
                    "requires the 'max_hp', 'base_dmg', and 'base_armour' keyword arguments to be "
                    "passed to __init__. This is a subclass of pf.Entity.",
    .tp_methods   = PyCombatableEntity_methods,
    .tp_base      = &PyEntity_type,
    .tp_getset    = PyCombatableEntity_getset,
    .tp_init      = (initproc)PyCombatableEntity_init,
};

KHASH_MAP_INIT_INT(PyObject, PyObject*)
static khash_t(PyObject) *s_uid_pyobj_table;
static PyObject          *s_loaded;

/*****************************************************************************/
/* STATIC FUNCTIONS                                                          */
/*****************************************************************************/

static bool s_has_super_method(const char *method_name, PyObject *type, PyObject *self)
{
    bool ret = false;

    PyObject *super_obj = PyObject_CallFunction((PyObject*)&PySuper_Type, "(OO)", type, self);
    if(!super_obj) {
        assert(PyErr_Occurred());
        goto fail_call_super;
    }

    ret = PyObject_HasAttrString(super_obj, method_name);
    Py_DECREF(super_obj);

fail_call_super:
    return ret;
}

static PyObject *s_call_super_method(const char *method_name, PyObject *type, 
                                     PyObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *ret = NULL;

    PyObject *super_obj = PyObject_CallFunction((PyObject*)&PySuper_Type, "(OO)", type, self);
    if(!super_obj) {
        assert(PyErr_Occurred());
        goto fail_call_super;
    }

    PyObject *method = PyObject_GetAttrString(super_obj, method_name);
    if(!method) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get super method."); 
        goto fail_get_method;
    }

    ret = PyObject_Call(method, args, kwds); 
    Py_DECREF(method);

fail_get_method:
    Py_DECREF(super_obj);
fail_call_super:
    return ret;
}

static PyObject *s_super_del(PyObject *self, PyTypeObject *type)
{
    if(!s_has_super_method("__del__", (PyObject*)type, self))
        Py_RETURN_NONE;

    PyObject *args = PyTuple_New(0);
    PyObject *ret = s_call_super_method("__del__", (PyObject*)type, self, args, NULL);
    Py_DECREF(args);
    return ret;
}

static PyObject *PyEntity_del(PyEntityObject *self)
{
    return s_super_del((PyObject*)self, &PyEntity_type);
}

static PyObject *PyEntity_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyEntityObject *self;
    const char *dirpath, *filename, *name;

    /* First, extract the first 3 args to handle the cases where subclasses get 
     * intialized with more arguments */
    PyObject *first_args = PyTuple_GetSlice(args, 0, 3);
    if(!first_args) {
        return NULL;
    }

    if(!PyArg_ParseTuple(first_args, "sss", &dirpath, &filename, &name)) {
        PyErr_SetString(PyExc_TypeError, "First 3 arguments must be strings.");
        return NULL;
    }
    Py_DECREF(first_args);

    PyObject *uidobj = NULL;
    uint32_t uid;

    if(kwds) {
        uidobj = PyDict_GetItemString(kwds, "__uid__");
    }
    if(uidobj && PyInt_Check(uidobj)) {
        uid = PyInt_AS_LONG(uidobj);
    }else {
        uid = Entity_NewUID();
    }

    struct entity *ent = AL_EntityFromPFObj(dirpath, filename, name, uid);
    if(!ent) {
        PyErr_SetString(PyExc_RuntimeError, "Unable to initialize pf.Entity from the given arguments.");
        return NULL;
    }

    self = (PyEntityObject*)type->tp_alloc(type, 0);
    if(!self) {
        PyErr_SetString(PyExc_RuntimeError, "Unable to allocate new pf.Entity.");
        AL_EntityFree(ent); 
        return NULL;
    }else{
        self->ent = ent; 
    }

    if(PyObject_IsInstance((PyObject*)self, (PyObject*)&PyCombatableEntity_type))
        self->ent->flags |= ENTITY_FLAG_COMBATABLE;

    G_AddEntity(self->ent, (vec3_t){0.0f, 0.0f, 0.0f});

    int ret;
    khiter_t k = kh_put(PyObject, s_uid_pyobj_table, ent->uid, &ret);
    assert(ret != -1 && ret != 0);
    kh_value(s_uid_pyobj_table, k) = (PyObject*)self;

    return (PyObject*)self;
}

static void PyEntity_dealloc(PyEntityObject *self)
{
    assert(self->ent);

    khiter_t k = kh_get(PyObject, s_uid_pyobj_table, self->ent->uid);
    assert(k != kh_end(s_uid_pyobj_table));
    kh_del(PyObject, s_uid_pyobj_table, k);

    G_RemoveEntity(self->ent);
    G_SafeFree(self->ent);

    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *PyEntity_get_name(PyEntityObject *self, void *closure)
{
    return Py_BuildValue("s", self->ent->name);
}

static int PyEntity_set_name(PyEntityObject *self, PyObject *value, void *closure)
{
    if(!PyObject_IsInstance(value, (PyObject*)&PyString_Type)){
        PyErr_SetString(PyExc_TypeError, "Argument must be a string.");
        return -1;
    }

    const char *s = PyString_AsString(value);
    if(strlen(s) >= sizeof(self->ent->name)){
        PyErr_SetString(PyExc_TypeError, "Name string is too long.");
        return -1;
    }

    strcpy(self->ent->name, s);
    return 0;
}

static PyObject *PyEntity_get_pos(PyEntityObject *self, void *closure)
{
    vec3_t pos = G_Pos_Get(self->ent->uid);
    return Py_BuildValue("(f,f,f)", pos.x, pos.y, pos.z);
}

static int PyEntity_set_pos(PyEntityObject *self, PyObject *value, void *closure)
{
    if(!PyTuple_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a tuple.");
        return -1;
    }
    
    vec3_t newpos;
    if(!PyArg_ParseTuple(value, "fff", 
        &newpos.raw[0], &newpos.raw[1], &newpos.raw[2])) {
        return -1;
    }

    G_Pos_Set(self->ent, newpos);
    return 0;
}

static PyObject *PyEntity_get_scale(PyEntityObject *self, void *closure)
{
    return Py_BuildValue("(f,f,f)", self->ent->scale.x, self->ent->scale.y, self->ent->scale.z);
}

static int PyEntity_set_scale(PyEntityObject *self, PyObject *value, void *closure)
{
    if(!PyTuple_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a tuple.");
        return -1;
    }
    
    if(!PyArg_ParseTuple(value, "fff", 
        &self->ent->scale.raw[0], &self->ent->scale.raw[1], &self->ent->scale.raw[2]))
        return -1;

    return 0;
}

static PyObject *PyEntity_get_rotation(PyEntityObject *self, void *closure)
{
    return Py_BuildValue("(f,f,f,f)", self->ent->rotation.x, self->ent->rotation.y, 
        self->ent->rotation.z, self->ent->rotation.w);
}

static int PyEntity_set_rotation(PyEntityObject *self, PyObject *value, void *closure)
{
    if(!PyTuple_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a tuple.");
        return -1;
    }
    
    if(!PyArg_ParseTuple(value, "ffff", 
        &self->ent->rotation.raw[0], &self->ent->rotation.raw[1],
        &self->ent->rotation.raw[2], &self->ent->rotation.raw[3]))
        return -1;

    return 0;
}

static PyObject *PyEntity_get_selectable(PyEntityObject *self, void *closure)
{
    if(self->ent->flags & ENTITY_FLAG_SELECTABLE)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static int PyEntity_set_selectable(PyEntityObject *self, PyObject *value, void *closure)
{
    int result = PyObject_IsTrue(value);

    if(-1 == result) {
        PyErr_SetString(PyExc_TypeError, "Argument must evaluate to True or False.");
        return -1;
    }else if(1 == result) {
        self->ent->flags |= ENTITY_FLAG_SELECTABLE;
    }else {
        self->ent->flags &= ~ENTITY_FLAG_SELECTABLE;
    }

    return 0;
}

static PyObject *PyEntity_get_selection_radius(PyEntityObject *self, void *closure)
{
    return Py_BuildValue("f", self->ent->selection_radius);
}

static int PyEntity_set_selection_radius(PyEntityObject *self, PyObject *value, void *closure)
{
    if(!PyFloat_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a float.");
        return -1;
    }

    self->ent->selection_radius = PyFloat_AsDouble(value);
    G_Move_UpdateSelectionRadius(self->ent, self->ent->selection_radius);
    return 0;
}

static PyObject *PyEntity_get_pfobj_path(PyEntityObject *self, void *closure)
{
    char buff[sizeof(self->ent->basedir) + sizeof(self->ent->filename) + 2];
    strcpy(buff, self->ent->basedir);
    strcat(buff, "/");
    strcat(buff, self->ent->filename);
    return PyString_FromString(buff); 
}

static PyObject *PyEntity_get_speed(PyEntityObject *self, void *closure)
{
    return PyFloat_FromDouble(self->ent->max_speed);
}

static int PyEntity_set_speed(PyEntityObject *self, PyObject *value, void *closure)
{
    if(!PyFloat_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "Speed attribute must be a float.");
        return -1;
    }

    self->ent->max_speed = PyFloat_AS_DOUBLE(value);
    return 0;
}

static PyObject *PyEntity_get_faction_id(PyEntityObject *self, void *closure)
{
    return Py_BuildValue("i", self->ent->faction_id);
}

static int PyEntity_set_faction_id(PyEntityObject *self, PyObject *value, void *closure)
{
    if(!PyInt_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "faction_id attribute must be an integer.");
        return -1;
    }

    int old = self->ent->faction_id;
    vec2_t xz_pos = G_Pos_GetXZ(self->ent->uid);

    self->ent->faction_id = PyInt_AS_LONG(value);

    G_Fog_UpdateVisionRange(xz_pos, old, self->ent->vision_range, 0.0f);
    G_Fog_UpdateVisionRange(xz_pos, self->ent->faction_id, 0.0f, self->ent->vision_range);
    return 0;
}

static PyObject *PyEntity_get_vision_range(PyEntityObject *self, void *closure)
{
    return Py_BuildValue("f", self->ent->vision_range);
}

static int PyEntity_set_vision_range(PyEntityObject *self, PyObject *value, void *closure)
{
    if(!PyFloat_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "vision_range attribute must be an float.");
        return -1;
    }

    float old = self->ent->vision_range;
    vec2_t xz_pos = G_Pos_GetXZ(self->ent->uid);

    self->ent->vision_range = PyFloat_AS_DOUBLE(value);
    G_Fog_UpdateVisionRange(xz_pos, self->ent->faction_id, old, self->ent->vision_range);
    return 0;
}

static PyObject *PyEntity_register(PyEntityObject *self, PyObject *args)
{
    enum eventtype event;
    PyObject *callable, *user_arg;

    if(!PyArg_ParseTuple(args, "iOO", &event, &callable, &user_arg)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be an integer and two objects.");
        return NULL;
    }

    if(!PyCallable_Check(callable)) {
        PyErr_SetString(PyExc_TypeError, "Second argument must be callable.");
        return NULL;
    }

    Py_INCREF(callable);
    Py_INCREF(user_arg);

    bool ret = E_Entity_ScriptRegister(event, self->ent->uid, callable, user_arg, G_RUNNING);
    assert(ret == true);
    Py_RETURN_NONE;
}

static PyObject *PyEntity_unregister(PyEntityObject *self, PyObject *args)
{
    enum eventtype event;
    PyObject *callable;

    if(!PyArg_ParseTuple(args, "iO", &event, &callable)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must an integer and one object.");
        return NULL;
    }

    if(!PyCallable_Check(callable)) {
        PyErr_SetString(PyExc_TypeError, "Second argument must be callable.");
        return NULL;
    }

    E_Entity_ScriptUnregister(event, self->ent->uid, callable);
    Py_RETURN_NONE;
}

static PyObject *PyEntity_notify(PyEntityObject *self, PyObject *args)
{
    enum eventtype event;
    PyObject *arg;

    if(!PyArg_ParseTuple(args, "iO", &event, &arg)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be an integer and one object.");
        return NULL;
    }

    Py_INCREF(arg);

    E_Entity_Notify(event, self->ent->uid, arg, ES_SCRIPT);
    Py_RETURN_NONE;
}

static PyObject *PyEntity_select(PyEntityObject *self)
{
    assert(self->ent);
    G_Sel_Add(self->ent);
    Py_RETURN_NONE;
}

static PyObject *PyEntity_deselect(PyEntityObject *self)
{
    assert(self->ent);
    G_Sel_Remove(self->ent);
    Py_RETURN_NONE;
}

static PyObject *PyEntity_stop(PyEntityObject *self)
{
    assert(self->ent);
    G_StopEntity(self->ent);
    Py_RETURN_NONE;
}

static PyObject *PyEntity_move(PyEntityObject *self, PyObject *args)
{
    vec2_t xz_pos;

    if(!PyArg_ParseTuple(args, "(ff)", &xz_pos.x, &xz_pos.z)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a tuple of 2 floats.");
        return NULL;
    }

    if(!G_PointInsideMap(xz_pos)) {
        PyErr_SetString(PyExc_RuntimeError, "The movement point must be within the map bounds.");
        return NULL;
    }

    G_Move_SetDest(self->ent, xz_pos);
    Py_RETURN_NONE;
}

static PyObject *PyEntity_pickle(PyEntityObject *self)
{
    bool status;
    PyObject *ret = NULL;

    SDL_RWops *stream = PFSDL_VectorRWOps();
    CHK_TRUE(stream, fail_alloc);

    PyObject *basedir = PyString_FromString(self->ent->basedir);
    CHK_TRUE(basedir, fail_pickle);
    status = S_PickleObjgraph(basedir, stream);
    Py_DECREF(basedir);
    CHK_TRUE(status, fail_pickle);

    PyObject *filename = PyString_FromString(self->ent->filename);
    CHK_TRUE(filename, fail_pickle);
    status = S_PickleObjgraph(filename, stream);
    Py_DECREF(filename);
    CHK_TRUE(status, fail_pickle);

    PyObject *name = PyString_FromString(self->ent->name);
    CHK_TRUE(name, fail_pickle);
    status = S_PickleObjgraph(name, stream);
    Py_DECREF(name);
    CHK_TRUE(status, fail_pickle);

    PyObject *uid = PyInt_FromLong(self->ent->uid);
    CHK_TRUE(uid, fail_pickle);
    status = S_PickleObjgraph(uid, stream);
    Py_DECREF(uid);
    CHK_TRUE(status, fail_pickle);

    vec3_t rawpos = G_Pos_Get(self->ent->uid);
    PyObject *pos = Py_BuildValue("(fff)", rawpos.x, rawpos.y, rawpos.z);
    CHK_TRUE(pos, fail_pickle);
    status = S_PickleObjgraph(pos, stream);
    Py_DECREF(pos);
    CHK_TRUE(status, fail_pickle);

    PyObject *scale = Py_BuildValue("(fff)", 
        self->ent->scale.x, self->ent->scale.y, self->ent->scale.z);
    CHK_TRUE(scale, fail_pickle);
    status = S_PickleObjgraph(scale, stream);
    Py_DECREF(scale);
    CHK_TRUE(status, fail_pickle);

    PyObject *rotation = Py_BuildValue("(ffff)", 
        self->ent->rotation.x, self->ent->rotation.y, self->ent->rotation.z, self->ent->rotation.w);
    CHK_TRUE(rotation, fail_pickle);
    status = S_PickleObjgraph(rotation, stream);
    Py_DECREF(rotation);
    CHK_TRUE(status, fail_pickle);

    PyObject *flags = Py_BuildValue("i", self->ent->flags);
    CHK_TRUE(flags, fail_pickle);
    status = S_PickleObjgraph(flags, stream);
    Py_DECREF(flags);
    CHK_TRUE(status, fail_pickle);

    PyObject *sel_radius = Py_BuildValue("f", self->ent->selection_radius);
    CHK_TRUE(sel_radius, fail_pickle);
    status = S_PickleObjgraph(sel_radius, stream);
    Py_DECREF(sel_radius);
    CHK_TRUE(status, fail_pickle);

    PyObject *max_speed = Py_BuildValue("f", self->ent->max_speed);
    CHK_TRUE(max_speed, fail_pickle);
    status = S_PickleObjgraph(max_speed, stream);
    Py_DECREF(max_speed);
    CHK_TRUE(status, fail_pickle);

    PyObject *faction_id = Py_BuildValue("i", self->ent->faction_id);
    CHK_TRUE(faction_id, fail_pickle);
    status = S_PickleObjgraph(faction_id, stream);
    Py_DECREF(faction_id);
    CHK_TRUE(status, fail_pickle);

    PyObject *vision_range = Py_BuildValue("f", self->ent->vision_range);
    CHK_TRUE(vision_range, fail_pickle);
    status = S_PickleObjgraph(vision_range, stream);
    Py_DECREF(vision_range);
    CHK_TRUE(status, fail_pickle);

    ret = PyString_FromStringAndSize(PFSDL_VectorRWOpsRaw(stream), SDL_RWsize(stream));

fail_pickle:
    SDL_RWclose(stream);
fail_alloc:
    return ret;
}

static PyObject *PyEntity_unpickle(PyObject *cls, PyObject *args)
{
    PyObject *ret = NULL;
    const char *str;
    Py_ssize_t len;
    int status;
    char tmp;

    if(!PyArg_ParseTuple(args, "s#", &str, &len)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a single string.");
        goto fail_args;
    }

    SDL_RWops *stream = SDL_RWFromConstMem(str, len);
    CHK_TRUE(stream, fail_args);

    PyObject *basedir = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    PyObject *filename = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    PyObject *name = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    PyObject *uid = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    if(!basedir || !filename || !name || !uid) {
        PyErr_SetString(PyExc_RuntimeError, "Could not unpickle internal state of pf.Entity instance");
        goto fail_unpickle;
    }

    PyObject *ent_args = Py_BuildValue("(OOO)", basedir, filename, name);
    PyObject *ent_kwargs = Py_BuildValue("{s:O}", "__uid__", uid);

    assert(((PyTypeObject*)cls)->tp_new);
    PyObject *entobj = ((PyTypeObject*)cls)->tp_new((struct _typeobject*)cls, ent_args, ent_kwargs);
    assert(entobj || PyErr_Occurred());

    Py_DECREF(ent_args);
    Py_DECREF(ent_kwargs);
    CHK_TRUE(entobj, fail_unpickle);

    PyObject *pos = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    PyObject *scale = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    PyObject *rotation = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    PyObject *flags = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    PyObject *sel_radius = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    PyObject *max_speed = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    PyObject *faction_id = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    PyObject *vision_range = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    if(!pos
    || !scale
    || !rotation
    || !flags 
    || !sel_radius 
    || !max_speed 
    || !faction_id
    || !vision_range) {
        PyErr_SetString(PyExc_RuntimeError, "Could not unpickle attributes of pf.Entity instance");
        goto fail_unpickle_atts;
    }

    struct entity *ent = ((PyEntityObject*)entobj)->ent;
    vec3_t rawpos;
    CHK_TRUE(PyArg_ParseTuple(pos, "fff", &rawpos.x, &rawpos.y, &rawpos.z), fail_unpickle_atts);
    G_Pos_Set(ent, rawpos);

    CHK_TRUE(PyArg_ParseTuple(scale, "fff", &ent->scale.x, &ent->scale.y, &ent->scale.z), fail_unpickle_atts);
    CHK_TRUE(PyArg_ParseTuple(rotation, "ffff", &ent->rotation.x, &ent->rotation.y, &ent->rotation.z, &ent->rotation.w), fail_unpickle_atts);

    uint32_t rawflags;
    CHK_TRUE((-1 != (rawflags = PyInt_AsLong(flags))), fail_unpickle_atts);
    G_SetStatic(ent, rawflags & ENTITY_FLAG_STATIC);
    ent->flags = rawflags;

    status = PyObject_SetAttrString(entobj, "selection_radius", sel_radius);
    CHK_TRUE(0 == status, fail_unpickle_atts);

    status = PyObject_SetAttrString(entobj, "speed", max_speed);
    CHK_TRUE(0 == status, fail_unpickle_atts);

    status = PyObject_SetAttrString(entobj, "faction_id", faction_id);
    CHK_TRUE(0 == status, fail_unpickle_atts);

    status = PyObject_SetAttrString(entobj, "vision_range", vision_range);
    CHK_TRUE(0 == status, fail_unpickle_atts);

    Py_ssize_t nread = SDL_RWseek(stream, 0, RW_SEEK_CUR);
    ret = Py_BuildValue("(Oi)", entobj, (int)nread);
    Py_DECREF(entobj);

fail_unpickle_atts:
    Py_XDECREF(pos);
    Py_XDECREF(scale);
    Py_XDECREF(rotation);
    Py_XDECREF(flags);
    Py_XDECREF(sel_radius);
    Py_XDECREF(max_speed);
    Py_XDECREF(faction_id);
    Py_XDECREF(vision_range);
fail_unpickle:
    Py_XDECREF(basedir);
    Py_XDECREF(filename);
    Py_XDECREF(name);
    Py_XDECREF(uid);
    SDL_RWclose(stream);
fail_args:
    return ret;
}

static PyObject *PyCombatableEntity_hold_position(PyCombatableEntityObject *self)
{
    assert(self->super.ent);

    if(!(self->super.ent->flags & ENTITY_FLAG_STATIC))
        G_StopEntity(self->super.ent);

    assert(self->super.ent->flags & ENTITY_FLAG_COMBATABLE);
    G_Combat_SetStance(self->super.ent, COMBAT_STANCE_HOLD_POSITION);
    Py_RETURN_NONE;
}

static PyObject *PyCombatableEntity_attack(PyCombatableEntityObject *self, PyObject *args)
{
    vec2_t xz_pos;

    if(!PyArg_ParseTuple(args, "(ff)", &xz_pos.x, &xz_pos.z)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a tuple of 2 floats.");
        return NULL;
    }

    if(!G_PointInsideMap(xz_pos)) {
        PyErr_SetString(PyExc_RuntimeError, "The movement point must be within the map bounds.");
        return NULL;
    }

    assert(self->super.ent->flags & ENTITY_FLAG_COMBATABLE);
    G_Combat_SetStance(self->super.ent, COMBAT_STANCE_AGGRESSIVE);

    if(!(self->super.ent->flags & ENTITY_FLAG_STATIC))
        G_Move_SetDest(self->super.ent, xz_pos);

    Py_RETURN_NONE;
}

static PyObject *PyCombatableEntity_pickle(PyCombatableEntityObject *self)
{
    PyObject *args = PyTuple_New(0);
    PyObject *kwargs = PyDict_New();
    CHK_TRUE(args && kwargs, fail_args);

    PyObject *ret = s_call_super_method("__pickle__", (PyObject*)&PyCombatableEntity_type, 
        (PyObject*)self, args, kwargs);

    Py_DECREF(args);
    Py_DECREF(kwargs);

    assert(PyString_Check(ret));
    SDL_RWops *stream = PFSDL_VectorRWOps();
    CHK_TRUE(stream, fail_stream);
    CHK_TRUE(SDL_RWwrite(stream, PyString_AS_STRING(ret), PyString_GET_SIZE(ret), 1), fail_stream);

    PyObject *max_hp = PyInt_FromLong(self->super.ent->max_hp);
    PyObject *base_dmg = PyInt_FromLong(G_Combat_GetBaseDamage(self->super.ent));
    PyObject *base_armour = PyFloat_FromDouble(G_Combat_GetBaseArmour(self->super.ent));
    PyObject *curr_hp = PyInt_FromLong(G_Combat_GetCurrentHP(self->super.ent));
    CHK_TRUE(max_hp && base_dmg && base_armour && curr_hp, fail_pickle);

    bool status;
    status = S_PickleObjgraph(max_hp, stream);
    CHK_TRUE(status, fail_pickle);
    status = S_PickleObjgraph(base_dmg, stream);
    CHK_TRUE(status, fail_pickle);
    status = S_PickleObjgraph(base_armour, stream);
    CHK_TRUE(status, fail_pickle);
    status = S_PickleObjgraph(curr_hp, stream);
    CHK_TRUE(status, fail_pickle);

    Py_DECREF(max_hp);
    Py_DECREF(base_dmg);
    Py_DECREF(base_armour);

    Py_DECREF(ret);
    ret = PyString_FromStringAndSize(PFSDL_VectorRWOpsRaw(stream), SDL_RWsize(stream));
    SDL_RWclose(stream);
    return ret;

fail_pickle:
    Py_XDECREF(max_hp);
    Py_XDECREF(base_dmg);
    Py_XDECREF(base_armour);
    Py_XDECREF(curr_hp);
    SDL_RWclose(stream);
fail_stream:
    Py_XDECREF(ret);
fail_args:
    return NULL;
}

static PyObject *PyCombatableEntity_unpickle(PyObject *cls, PyObject *args)
{
    char tmp;
    PyObject *max_hp = NULL, *base_dmg = NULL, *base_armour = NULL, *curr_hp = NULL;
    PyObject *ret = NULL;
    int status;

    PyObject *kwargs = PyDict_New();
    if(!kwargs)
        return NULL;

    PyObject *tuple = s_call_super_method("__unpickle__", (PyObject*)&PyCombatableEntity_type, cls, args, kwargs);
    Py_DECREF(kwargs);
    if(!tuple)
        return NULL;

    PyObject *ent;
    int nread; 
    if(!PyArg_ParseTuple(tuple, "Oi", &ent, &nread)) {
        Py_DECREF(tuple);
        return NULL;
    }
    Py_INCREF(ent);
    Py_DECREF(tuple);

    SDL_RWops *stream = PFSDL_VectorRWOps();
    CHK_TRUE(stream, fail_stream);

    PyObject *str = PyTuple_GET_ITEM(args, 0); /* borrowed */
    CHK_TRUE(SDL_RWwrite(stream, PyString_AS_STRING(str), PyString_GET_SIZE(str), 1), fail_unpickle);
    SDL_RWseek(stream, nread, RW_SEEK_SET);

    max_hp = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    status = PyObject_SetAttrString(ent, "max_hp", max_hp);
    CHK_TRUE(0 == status, fail_unpickle);

    base_dmg = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    status = PyObject_SetAttrString(ent, "base_dmg", base_dmg);
    CHK_TRUE(0 == status, fail_unpickle);

    base_armour = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    status = PyObject_SetAttrString(ent, "base_armour", base_armour);
    CHK_TRUE(0 == status, fail_unpickle);

    curr_hp = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    CHK_TRUE(PyInt_Check(curr_hp), fail_unpickle);
    G_Combat_SetHP(((PyEntityObject*)ent)->ent, PyInt_AS_LONG(curr_hp));

    nread = SDL_RWseek(stream, 0, RW_SEEK_CUR);
    ret = Py_BuildValue("Oi", ent, nread);

fail_unpickle:
    Py_XDECREF(max_hp);
    Py_XDECREF(base_dmg);
    Py_XDECREF(base_armour);
    Py_XDECREF(curr_hp);
    SDL_RWclose(stream);
fail_stream:
    Py_DECREF(ent);
    return ret;
}

static int PyAnimEntity_init(PyAnimEntityObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *idle_clip;

    if(!kwds || ((idle_clip = PyDict_GetItemString(kwds, "idle_clip")) == NULL)) {
        PyErr_SetString(PyExc_TypeError, "'idle_clip' keyword argument required for initializing pf.AnimEntity types.");
        return -1;
    }

    if(!PyString_Check(idle_clip)) {
        PyErr_SetString(PyExc_TypeError, "'idle_clip' keyword argument must be a string.");
        return -1; 
    }

    if(!A_HasClip(self->super.ent, PyString_AS_STRING(idle_clip))) {
        char errbuff[256];
        pf_snprintf(errbuff, sizeof(errbuff), "%s instance has no animation clip named '%s'.", 
            self->super.ent->filename, PyString_AS_STRING(idle_clip));
        PyErr_SetString(PyExc_RuntimeError, errbuff);
        return -1;
    }

    A_SetIdleClip(self->super.ent, PyString_AS_STRING(idle_clip), 24);

    /* Call the next __init__ method in the MRO. This is required for all __init__ calls in the 
     * MRO to complete in cases when this class is one of multiple base classes of another type. 
     * This allows this type to be used as one of many mix-in bases. */
    PyObject *ret = s_call_super_method("__init__", (PyObject*)&PyAnimEntity_type, (PyObject*)self, args, kwds);
    if(!ret)
        return -1; /* Exception already set */
    Py_DECREF(ret);
    return 0;
}

static PyObject *PyAnimEntity_del(PyAnimEntityObject *self)
{
    return s_super_del((PyObject*)self, &PyAnimEntity_type);
}

static PyObject *PyAnimEntity_play_anim(PyAnimEntityObject *self, PyObject *args, PyObject *kwds)
{
    const char *clipname;
    if(!PyArg_ParseTuple(args, "s", &clipname)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a string.");
        return NULL;
    }

    enum anim_mode mode = ANIM_MODE_LOOP; /* default */
    PyObject *mode_obj;

    if(kwds && (mode_obj = PyDict_GetItemString(kwds, "mode"))) {
    
        if(!PyInt_Check(mode_obj)
        || (mode = PyInt_AS_LONG(mode_obj)) > ANIM_MODE_ONCE_HIDE_ON_FINISH) {
        
            PyErr_SetString(PyExc_TypeError, "Mode kwarg must be a valid animation mode (int).");
            return NULL;
        }
    }

    if(!A_HasClip(self->super.ent, clipname)) {
        char errbuff[256];
        pf_snprintf(errbuff, sizeof(errbuff), "%s instance has no animation clip named '%s'.", 
            self->super.ent->filename, clipname);
        PyErr_SetString(PyExc_RuntimeError, errbuff);
        return NULL;
    }

    A_SetActiveClip(self->super.ent, clipname, mode, 24);
    Py_RETURN_NONE;
}

static PyObject *PyAnimEntity_get_anim(PyAnimEntityObject *self)
{
    return PyString_FromString(A_GetCurrClip(self->super.ent));
}

static PyObject *PyAnimEntity_pickle(PyAnimEntityObject *self)
{
    PyObject *args = PyTuple_New(0);
    PyObject *kwargs = PyDict_New();
    CHK_TRUE(args && kwargs, fail_args);

    PyObject *ret = s_call_super_method("__pickle__", (PyObject*)&PyAnimEntity_type, 
        (PyObject*)self, args, kwargs);

    Py_DECREF(args);
    Py_DECREF(kwargs);

    assert(PyString_Check(ret));
    SDL_RWops *stream = PFSDL_VectorRWOps();
    CHK_TRUE(stream, fail_stream);
    CHK_TRUE(SDL_RWwrite(stream, PyString_AS_STRING(ret), PyString_GET_SIZE(ret), 1), fail_stream);

    PyObject *idle_clip = PyString_FromString(A_GetIdleClip(self->super.ent));
    CHK_TRUE(idle_clip, fail_pickle);
    bool status = S_PickleObjgraph(idle_clip, stream);
    Py_DECREF(idle_clip);
    CHK_TRUE(status, fail_pickle);

    Py_DECREF(ret);
    ret = PyString_FromStringAndSize(PFSDL_VectorRWOpsRaw(stream), SDL_RWsize(stream));
    SDL_RWclose(stream);
    return ret;

fail_pickle:
    SDL_RWclose(stream);
fail_stream:
    Py_XDECREF(ret);
fail_args:
    return NULL;
}

static PyObject *PyAnimEntity_unpickle(PyObject *cls, PyObject *args)
{
    char tmp;
    PyObject *ret = NULL;

    PyObject *kwargs = PyDict_New();
    if(!kwargs)
        return NULL;

    PyObject *tuple = s_call_super_method("__unpickle__", (PyObject*)&PyAnimEntity_type, cls, args, kwargs);
    Py_DECREF(kwargs);
    if(!tuple)
        return NULL;

    PyObject *ent;
    int nread; 
    if(!PyArg_ParseTuple(tuple, "Oi", &ent, &nread)) {
        Py_DECREF(tuple);
        return NULL;
    }
    Py_INCREF(ent);
    Py_DECREF(tuple);

    SDL_RWops *stream = PFSDL_VectorRWOps();
    CHK_TRUE(stream, fail_stream);

    PyObject *str = PyTuple_GET_ITEM(args, 0); /* borrowed */
    CHK_TRUE(SDL_RWwrite(stream, PyString_AS_STRING(str), PyString_GET_SIZE(str), 1), fail_unpickle);

    SDL_RWseek(stream, nread, RW_SEEK_SET);
    PyObject *idle_clip = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */
    CHK_TRUE(idle_clip, fail_unpickle);
    CHK_TRUE(PyString_Check(idle_clip), fail_parse);

    nread = SDL_RWseek(stream, 0, RW_SEEK_CUR);
    A_SetIdleClip(((PyAnimEntityObject*)ent)->super.ent, PyString_AS_STRING(idle_clip), 24);

    ret = Py_BuildValue("Oi", ent, nread);

fail_parse:
    Py_DECREF(idle_clip);
fail_unpickle:
    SDL_RWclose(stream);
fail_stream:
    Py_DECREF(ent);
    return ret;
}

static int PyCombatableEntity_init(PyCombatableEntityObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *max_hp, *base_dmg, *base_armour;

    if(!kwds 
    || ((max_hp = PyDict_GetItemString(kwds, "max_hp")) == NULL)
    || ((base_dmg = PyDict_GetItemString(kwds, "base_dmg")) == NULL)
    || ((base_armour = PyDict_GetItemString(kwds, "base_armour")) == NULL)) {
        PyErr_SetString(PyExc_TypeError, "'max_hp', 'base_dmg', and 'base_armour' keyword arguments "
            "required for initializing pf.CombatableEntity types.");
        return -1;
    }

    assert(self->super.ent->flags & ENTITY_FLAG_COMBATABLE);
    if((PyCombatableEntity_set_max_hp(self, max_hp, NULL) != 0)
    || (PyCombatableEntity_set_base_dmg(self, base_dmg, NULL) != 0)
    || (PyCombatableEntity_set_base_armour(self, base_armour, NULL) != 0))
        return -1; /* Exception already set */ 

    G_Combat_SetHP(self->super.ent, PyInt_AS_LONG(max_hp));

    /* Call the next __init__ method in the MRO. This is required for all __init__ calls in the 
     * MRO to complete in cases when this class is one of multiple base classes of another type. 
     * This allows this type to be used as one of many mix-in bases. */
    PyObject *ret = s_call_super_method("__init__", (PyObject*)&PyCombatableEntity_type, (PyObject*)self, args, kwds);
    if(!ret)
        return -1; /* Exception already set */
    Py_DECREF(ret);
    return 0;
}

static PyObject *PyCombatableEntity_del(PyCombatableEntityObject *self)
{
    return s_super_del((PyObject*)self, &PyCombatableEntity_type);
}

static PyObject *PyCombatableEntity_get_max_hp(PyCombatableEntityObject *self, void *closure)
{
    return Py_BuildValue("i", self->super.ent->max_hp);
}

static int PyCombatableEntity_set_max_hp(PyCombatableEntityObject *self, PyObject *value, void *closure)
{
    if(!PyInt_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "max_hp attribute must be an integer.");
        return -1;
    }
    
    int max_hp = PyInt_AS_LONG(value);
    if(max_hp <= 0) {
        PyErr_SetString(PyExc_RuntimeError, "max_hp must be greater than 0.");
        return -1;
    }

    self->super.ent->max_hp = max_hp;
    return 0;
}

static PyObject *PyCombatableEntity_get_base_dmg(PyCombatableEntityObject *self, void *closure)
{
    return PyInt_FromLong(G_Combat_GetBaseDamage(self->super.ent));
}

static int PyCombatableEntity_set_base_dmg(PyCombatableEntityObject *self, PyObject *value, void *closure)
{
    if(!PyInt_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "base_dmg attribute must be an integer.");
        return -1;
    }
    
    int base_dmg = PyInt_AS_LONG(value);
    if(base_dmg < 0) {
        PyErr_SetString(PyExc_RuntimeError, "base_dmg must be greater than or equal to 0.");
        return -1;
    }

    G_Combat_SetBaseDamage(self->super.ent, base_dmg);
    return 0;
}

static PyObject *PyCombatableEntity_get_base_armour(PyCombatableEntityObject *self, void *closure)
{
    return PyFloat_FromDouble(G_Combat_GetBaseArmour(self->super.ent));
}

static int PyCombatableEntity_set_base_armour(PyCombatableEntityObject *self, PyObject *value, void *closure)
{
    if(!PyFloat_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "base_armour attribute must be a float.");
        return -1;
    }
    
    float base_armour = PyFloat_AS_DOUBLE(value);
    if(base_armour < 0.0f || base_armour > 1.0f) {
        PyErr_SetString(PyExc_RuntimeError, "base_armour must be in the range of [0.0, 1.0].");
        return -1;
    }

    G_Combat_SetBaseArmour(self->super.ent, base_armour);
    return 0;
}

static PyObject *s_obj_from_attr(const struct attr *attr)
{
    switch(attr->type){
    case TYPE_STRING:   return Py_BuildValue("s", attr->val.as_string);
    case TYPE_FLOAT:    return Py_BuildValue("f", attr->val.as_float);
    case TYPE_INT:      return Py_BuildValue("i", attr->val.as_int);
    case TYPE_VEC3:     return Py_BuildValue("(f,f,f)", attr->val.as_vec3.x, attr->val.as_vec3.y, attr->val.as_vec3.z);
    case TYPE_QUAT:     return Py_BuildValue("(f,f,f,f)", 
                               attr->val.as_quat.x, attr->val.as_quat.y, attr->val.as_quat.z, attr->val.as_quat.w);
    case TYPE_BOOL:     return Py_BuildValue("i", attr->val.as_bool);
    default: assert(0); return NULL;
    }
}

static PyObject *s_tuple_from_attr_vec(const vec_attr_t *attr_vec)
{
    PyObject *ret = PyTuple_New(vec_size(attr_vec));
    if(!ret)
        return NULL;

    for(int i = 0; i < vec_size(attr_vec); i++) {
        PyTuple_SetItem(ret, i, s_obj_from_attr(&vec_AT(attr_vec, i)));
    }

    return ret;
}

static PyObject *s_entity_from_atts(const char *path, const char *name, const khash_t(attr) *attr_table)
{
    PyObject *ret;

    char copy[256];
    if(strlen(path) >= sizeof(copy)) 
        return NULL;
    strcpy(copy, path);
    char *end = copy + strlen(path);
    assert(*end == '\0');
    while(end > copy && *end != '/')
        end--;
    if(end == copy)
        return NULL;
    *end = '\0';
    const char *filename = end + 1;

    khiter_t k = kh_get(attr, attr_table, "animated");
    if(k == kh_end(attr_table))
        return NULL;
    bool anim = kh_value(attr_table, k).val.as_bool;

    PyObject *args = PyTuple_New(anim ? 4 : 3);
    if(!args)
        return NULL;

    PyTuple_SetItem(args, 0, PyString_FromString(copy));
    PyTuple_SetItem(args, 1, PyString_FromString(filename));
    PyTuple_SetItem(args, 2, PyString_FromString(name));

    if(anim) {
        k = kh_get(attr, attr_table, "idle_clip");
        if(k == kh_end(attr_table)) {
            Py_DECREF(args);
            return NULL;
        }

        PyObject *kwargs = PyDict_New();
        if(!kwargs) {
            Py_DECREF(args);
            return NULL;
        }

        PyDict_SetItemString(kwargs, "idle_clip", s_obj_from_attr(&kh_value(attr_table, k)));
        ret = PyObject_Call((PyObject*)&PyAnimEntity_type, args, kwargs);
        Py_DECREF(kwargs);
    }else{
        ret = PyObject_CallObject((PyObject*)&PyEntity_type, args);
    }

    Py_DECREF(args);
    return ret;
}

static PyObject *s_new_custom_class(const char *name, const vec_attr_t *construct_args)
{
    PyObject *sys_mod_dict = PyImport_GetModuleDict();
    PyObject *modules = PyMapping_Values(sys_mod_dict);
    PyObject *class = NULL;
    for(int i = 0; i < PyList_Size(modules); i++) {
        if(PyObject_HasAttrString(PyList_GetItem(modules, i), name)) {
            class = PyObject_GetAttrString(PyList_GetItem(modules, i), name);
            break;
        }
    }
    Py_DECREF(modules);
    if(!class)
        return NULL;

    PyObject *args = s_tuple_from_attr_vec(construct_args);
    if(!args){
        Py_DECREF(class);
        return NULL;
    }
    PyObject *ret = PyObject_CallObject(class, args);
    if(!ret) {
        PyErr_Print();
        exit(EXIT_FAILURE);
    }

    Py_DECREF(class);
    Py_DECREF(args);

    return ret;
}

/*****************************************************************************/
/* EXTERN FUNCTIONS                                                          */
/*****************************************************************************/

void S_Entity_PyRegister(PyObject *module)
{
    if(PyType_Ready(&PyEntity_type) < 0)
        return;
    Py_INCREF(&PyEntity_type);
    PyModule_AddObject(module, "Entity", (PyObject*)&PyEntity_type);

    if(PyType_Ready(&PyAnimEntity_type) < 0)
        return;
    Py_INCREF(&PyAnimEntity_type);
    PyModule_AddObject(module, "AnimEntity", (PyObject*)&PyAnimEntity_type);

    if(PyType_Ready(&PyCombatableEntity_type) < 0)
        return;
    Py_INCREF(&PyCombatableEntity_type);
    PyModule_AddObject(module, "CombatableEntity", (PyObject*)&PyCombatableEntity_type);
}

bool S_Entity_Init(void)
{
    s_loaded = PyList_New(0);
    if(!s_loaded)
        return false;

    s_uid_pyobj_table = kh_init(PyObject);
    if(!s_uid_pyobj_table) {
        Py_DECREF(s_loaded);
        return false;
    }
    return true;
}

void S_Entity_Shutdown(void)
{
    Py_DECREF(s_loaded);
    kh_destroy(PyObject, s_uid_pyobj_table);
}

PyObject *S_Entity_ObjForUID(uint32_t uid)
{
    khiter_t k = kh_get(PyObject, s_uid_pyobj_table, uid);
    if(k == kh_end(s_uid_pyobj_table))
        return NULL;

    return kh_value(s_uid_pyobj_table, k);
}

script_opaque_t S_Entity_ObjFromAtts(const char *path, const char *name,
                                     const khash_t(attr) *attr_table, 
                                     const vec_attr_t *construct_args)
{
    khiter_t k;
    PyObject *ret = NULL;

    /* First, attempt to make an instance of the custom class specified in the 
     * attributes dictionary, if the key is present. */
    if((k = kh_get(attr, attr_table, "class")) != kh_end(attr_table)) {
        ret = s_new_custom_class(kh_value(attr_table, k).val.as_string, construct_args);
    }

    /* If we could not make a custom class, fall back to instantiating a basic entity */
    if(!ret){
        ret = s_entity_from_atts(path, name, attr_table); 
    }

    if(!ret){
        return NULL;
    }

    if((k = kh_get(attr, attr_table, "static")) != kh_end(attr_table)) {
        G_SetStatic(((PyEntityObject*)ret)->ent, kh_value(attr_table, k).val.as_bool);
    }

    if((k = kh_get(attr, attr_table, "collision")) != kh_end(attr_table)) {
        if(kh_value(attr_table, k).val.as_bool)
            ((PyEntityObject*)ret)->ent->flags |= ENTITY_FLAG_COLLISION;
        else
            ((PyEntityObject*)ret)->ent->flags &= ~ENTITY_FLAG_COLLISION;
    }

    if(PyObject_HasAttrString(ret, "selection_radius") 
    && (k = kh_get(attr, attr_table, "selection_radius")) != kh_end(attr_table))
        PyObject_SetAttrString(ret, "selection_radius", s_obj_from_attr(&kh_value(attr_table, k)));

    if(PyObject_HasAttrString(ret, "pos") 
    && (k = kh_get(attr, attr_table, "position")) != kh_end(attr_table))
        PyObject_SetAttrString(ret, "pos", s_obj_from_attr(&kh_value(attr_table, k)));

    if(PyObject_HasAttrString(ret, "scale") 
    && (k = kh_get(attr, attr_table, "scale")) != kh_end(attr_table))
        PyObject_SetAttrString(ret, "scale", s_obj_from_attr(&kh_value(attr_table, k)));

    if(PyObject_HasAttrString(ret, "rotation") 
    && (k = kh_get(attr, attr_table, "rotation")) != kh_end(attr_table))
        PyObject_SetAttrString(ret, "rotation", s_obj_from_attr(&kh_value(attr_table, k)));

    if(PyObject_HasAttrString(ret, "selectable") 
    && (k = kh_get(attr, attr_table, "selectable")) != kh_end(attr_table))
        PyObject_SetAttrString(ret, "selectable", s_obj_from_attr(&kh_value(attr_table, k)));

    if(PyObject_HasAttrString(ret, "faction_id") 
    && (k = kh_get(attr, attr_table, "faction_id")) != kh_end(attr_table))
        PyObject_SetAttrString(ret, "faction_id", s_obj_from_attr(&kh_value(attr_table, k)));

    if(PyObject_HasAttrString(ret, "vision_range") 
    && (k = kh_get(attr, attr_table, "vision_range")) != kh_end(attr_table))
        PyObject_SetAttrString(ret, "vision_range", s_obj_from_attr(&kh_value(attr_table, k)));

    PyList_Append(s_loaded, ret);
    Py_DECREF(ret);
    return ret;
}

PyObject *S_Entity_GetLoaded(void)
{
    PyObject *ret = s_loaded;
    if(!ret)
        return NULL;

    s_loaded = PyList_New(0);
    assert(s_loaded);
    
    return ret;
}

