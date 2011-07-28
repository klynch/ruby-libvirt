/*
 * stream.c: virStream methods
 *
 * Copyright (C) 2007,2010 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

#include <ruby.h>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include "common.h"
#include "connect.h"
#include "extconf.h"

#if HAVE_TYPE_VIRSTREAMPTR
static VALUE c_stream;

static void stream_free(void *s) {
    generic_free(Stream, s);
}

virStreamPtr stream_get(VALUE s) {
    generic_get(Stream, s);
}

VALUE stream_new(virStreamPtr s, VALUE conn) {
    return generic_new(c_stream, s, conn, stream_free);
}

/*
 * call-seq:
 *   stream.send(buffer) -> Fixnum
 *
 * Call +virStreamSend+[http://www.libvirt.org/html/libvirt-libvirt.html#virStreamSend]
 * to send the data in buffer out to the stream.  The return value is the
 * number of bytes sent, which may be less than the size of the buffer.  If
 * an error occurred, -1 is returned.  If the transmit buffers are full and the
 * stream is marked non-blocking, returns -2.
 */
static VALUE libvirt_stream_send(VALUE s, VALUE buffer) {
    int ret;

    StringValue(buffer);

    ret = virStreamSend(stream_get(s), RSTRING_PTR(buffer),
                        RSTRING_LEN(buffer));
    _E(ret == -1, create_error(e_RetrieveError, "virStreamSend", conn(s)));

    return INT2NUM(ret);
}

struct stream_recv_args {
    int ret;
    char *data;
};

static VALUE stream_recv_array(VALUE input) {
    VALUE result;
    struct stream_recv_args *args = (struct stream_recv_args *)input;

    result = rb_ary_new();

    rb_ary_push(result, INT2NUM(args->ret));
    rb_ary_push(result, rb_str_new(args->data, args->ret));

    return result;
}

/*
 * call-seq:
 *   stream.recv(bytes) -> [return_value, data]
 *
 * Call +virStreamRecv+[http://www.libvirt.org/html/libvirt-libvirt.html#virStreamRecv]
 * to receive up to bytes amount of data from the stream.  The return is an
 * array with two elements; the return code from the virStreamRecv call and
 * the data (as a String) read from the stream.  If an error occurred, the
 * return_value is set to -1.  If there is no data pending and the stream is
 * marked as non-blocking, return_value is set to -2.
 */
static VALUE libvirt_stream_recv(VALUE s, VALUE bytes) {
    char *data;
    int ret;
    int exception = 0;
    VALUE result;
    struct stream_recv_args args;

    data = ALLOC_N(char, NUM2INT(bytes));

    ret = virStreamRecv(stream_get(s), data, NUM2INT(bytes));
    if (ret == -1) {
        xfree(data);
        rb_exc_raise(create_error(e_RetrieveError, "virStreamRecv", conn(s)));
    }

    args.ret = ret;
    args.data = data;
    result = rb_protect(stream_recv_array, (VALUE)&args, &exception);
    if (exception) {
        xfree(data);
        rb_jump_tag(exception);
    }

    xfree(data);
    return result;
}

static int internal_sendall(virStreamPtr st, char *data, size_t nbytes,
                            void *opaque) {
    VALUE result;
    VALUE retcode, buffer;

    result = rb_yield_values(2, (VALUE)opaque, INT2NUM(nbytes));

    if (TYPE(result) != T_ARRAY)
        rb_raise(rb_eTypeError, "wrong type (expected Array)");

    if (RARRAY_LEN(result) != 2)
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 2)",
                 RARRAY_LEN(result));

    retcode = rb_ary_entry(result, 0);
    buffer = rb_ary_entry(result, 1);

    if (NUM2INT(retcode) < 0)
        return NUM2INT(retcode);

    StringValue(buffer);

    if (RSTRING_LEN(buffer) > nbytes)
        rb_raise(rb_eArgError, "asked for %d bytes, block returned %d", nbytes,
                 RSTRING_LEN(buffer));

    memcpy(data, RSTRING_PTR(buffer), RSTRING_LEN(buffer));

    return NUM2INT(retcode);
}

/*
 * call-seq:
 *   stream.sendall(opaque=nil){|opaque, nbytes| send block} -> nil
 *
 * Call +virStreamSendAll+[http://www.libvirt.org/html/libvirt-libvirt.html#virStreamSendAll]
 * to send the entire data stream.  The send block is required and is executed
 * one or more times to send data.  Each invocation of the send block yields
 * the opaque data passed into the initial call and the number of bytes this
 * iteration is prepared to handle.  The send block should return an array of
 * 2 elements; the first element should be the return code from the block
 * (-1 for error, 0 otherwise), and the second element should be the data
 * that the block prepared to send.
 */
static VALUE libvirt_stream_sendall(int argc, VALUE *argv, VALUE s) {
    VALUE opaque;
    int ret;

    if (!rb_block_given_p())
        rb_raise(rb_eRuntimeError, "A block must be provided");

    rb_scan_args(argc, argv, "01", &opaque);

    ret = virStreamSendAll(stream_get(s), internal_sendall, (void *)opaque);
    _E(ret < 0, create_error(e_RetrieveError, "virStreamSendAll", conn(s)));

    return Qnil;
}

static int internal_recvall(virStreamPtr st, const char *buf, size_t nbytes,
                            void *opaque) {
    VALUE result;

    result = rb_yield_values(2, rb_str_new(buf, nbytes), (VALUE)opaque);

    if (TYPE(result) != T_FIXNUM)
        rb_raise(rb_eArgError, "wrong type (expected an integer)");

    return NUM2INT(result);
}

/*
 * call-seq:
 *   stream.recvall(opaque){|data, opaque| receive block} -> nil
 *
 * Call +virStreamRecvAll+[http://www.libvirt.org/html/libvirt-libvirt.html#virStreamRecvAll]
 * to receive the entire data stream.  The receive block is required and is
 * called one or more times to receive data.  Each invocation of the receive
 * block yields the data received and the opaque data passed into the initial
 * call.  The block should return -1 if an error occurred and 0 otherwise.
 */
static VALUE libvirt_stream_recvall(int argc, VALUE *argv, VALUE s) {
    VALUE opaque;
    int ret;

    if (!rb_block_given_p())
        rb_raise(rb_eRuntimeError, "A block must be provided");

    rb_scan_args(argc, argv, "01", &opaque);

    ret = virStreamRecvAll(stream_get(s), internal_recvall, (void *)opaque);
    _E(ret < 0, create_error(e_RetrieveError, "virStreamRecvAll", conn(s)));

    return Qnil;
}

static void stream_event_callback(virStreamPtr st, int events, void *opaque) {
    VALUE passthrough = (VALUE)opaque;
    VALUE cb;
    VALUE cb_opaque;
    VALUE news;
    VALUE s;

    if (TYPE(passthrough) != T_ARRAY)
        rb_raise(rb_eTypeError,
                 "wrong domain event lifecycle callback argument type (expected Array)");

    if (RARRAY_LEN(passthrough) != 3)
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 3)",
                 RARRAY_LEN(passthrough));

    cb = rb_ary_entry(passthrough, 0);
    cb_opaque = rb_ary_entry(passthrough, 1);
    s = rb_ary_entry(passthrough, 2);

    news = stream_new(st, conn_attr(s));
    if (strcmp(rb_obj_classname(cb), "Symbol") == 0)
        rb_funcall(rb_class_of(cb), rb_to_id(cb), 3, news, INT2NUM(events),
                   cb_opaque);
    else if (strcmp(rb_obj_classname(cb), "Proc") == 0)
        rb_funcall(cb, rb_intern("call"), 3, news, INT2NUM(events), cb_opaque);
    else
        rb_raise(rb_eTypeError,
                 "wrong stream event callback (expected Symbol or Proc)");
}

/*
 * call-seq:
 *   stream.event_add_callback(events, callback, opaque=nil) -> nil
 *
 * Call +virStreamEventAddCallback+[http://www.libvirt.org/html/libvirt-libvirt.html#virStreamEventAddCallback]
 * to register a callback to be notified when a stream becomes readable or
 * writeable.  The events parameter is an integer representing the events the
 * user is interested in; it should be one or more of EVENT_READABLE,
 * EVENT_WRITABLE, EVENT_ERROR, and EVENT_HANGUP, ORed together.  The callback
 * can either be a Symbol (that is the name of a method to callback) or a Proc.
 * The callback should accept 3 parameters: a pointer to the Stream object
 * itself, the integer that represents the events that actually occurred, and
 * an opaque pointer that was (optionally) passed into
 * stream.event_add_callback to begin with.
 */
static VALUE libvirt_stream_event_add_callback(int argc, VALUE *argv, VALUE s) {
    VALUE events;
    VALUE callback;
    VALUE opaque;
    VALUE passthrough;
    int ret;

    rb_scan_args(argc, argv, "21", &events, &callback, &opaque);

    if (!is_symbol_or_proc(callback))
        rb_raise(rb_eTypeError, "wrong argument type (expected Symbol or Proc)");

    passthrough = rb_ary_new();
    rb_ary_store(passthrough, 0, callback);
    rb_ary_store(passthrough, 1, opaque);
    rb_ary_store(passthrough, 2, s);

    ret = virStreamEventAddCallback(stream_get(s), NUM2INT(events),
                                    stream_event_callback, (void *)passthrough,
                                    NULL);
    _E(ret < 0, create_error(e_RetrieveError, "virStreamEventAddCallback",
                             conn(s)));

    return Qnil;
}

/*
 * call-seq:
 *   stream.event_update_callback(events) -> nil
 *
 * Call +virStreamEventUpdateCallback+[http://www.libvirt.org/html/libvirt-libvirt.html#virStreamEventUpdateCallback]
 * to change the events that the event callback is looking for.  The events
 * parameter is an integer representing the events the user is interested in;
 * it should be one or more of EVENT_READABLE, EVENT_WRITABLE, EVENT_ERROR,
 * and EVENT_HANGUP, ORed together.
 */
static VALUE libvirt_stream_event_update_callback(VALUE s, VALUE events) {
    int ret;

    ret = virStreamEventUpdateCallback(stream_get(s), NUM2INT(events));
    _E(ret < 0, create_error(e_RetrieveError, "virStreamEventUpdateCallback",
                             conn(s)));

    return Qnil;
}

/*
 * call-seq:
 *   stream.event_remove_callback -> nil
 *
 * Call +virStreamEventRemoveCallback+[http://www.libvirt.org/html/libvirt-libvirt.html#virStreamEventRemoveCallback]
 * to remove the event callback currently registered to this stream.
 */
static VALUE libvirt_stream_event_remove_callback(VALUE s) {
    int ret;

    ret = virStreamEventRemoveCallback(stream_get(s));
    _E(ret < 0, create_error(e_RetrieveError, "virStreamEventRemoveCallback",
                             conn(s)));

    return Qnil;
}

/*
 * call-seq:
 *   stream.finish -> nil
 *
 * Call +virStreamFinish+[http://www.libvirt.org/html/libvirt-libvirt.html#virStreamFinish]
 * to finish this stream.  Finish is typically used when the stream is no
 * longer needed and needs to be cleaned up.
 */
static VALUE libvirt_stream_finish(VALUE s) {
    gen_call_void(virStreamFinish, conn(s), stream_get(s));
}

/*
 * call-seq:
 *   stream.abort -> nil
 *
 * Call +virStreamAbort+[http://www.libvirt.org/html/libvirt-libvirt.html#virStreamAbort]
 * to abort this stream.  Abort is typically used when something on the stream
 * has failed, and the stream needs to be cleaned up.
 */
static VALUE libvirt_stream_abort(VALUE s) {
    gen_call_void(virStreamAbort, conn(s), stream_get(s));
}

/*
 * call-seq:
 *   stream.free -> nil
 *
 * Call +virStreamFree+[http://www.libvirt.org/html/libvirt-libvirt.html#virStreamFree]
 * to free this stream.  The object will no longer be valid after this call.
 */
static VALUE libvirt_stream_free(VALUE s) {
    gen_call_free(Stream, s);
}
#endif

/*
 * Class Libvirt::Domain
 */
void init_stream()
{
#if HAVE_TYPE_VIRSTREAMPTR
    c_stream = rb_define_class_under(m_libvirt, "Stream", rb_cObject);

    rb_define_attr(c_stream, "connection", 1, 0);

    rb_define_const(c_stream, "NONBLOCK", INT2NUM(VIR_STREAM_NONBLOCK));

    rb_define_const(c_stream, "EVENT_READABLE",
                    INT2NUM(VIR_STREAM_EVENT_READABLE));
    rb_define_const(c_stream, "EVENT_WRITABLE",
                    INT2NUM(VIR_STREAM_EVENT_WRITABLE));
    rb_define_const(c_stream, "EVENT_ERROR", INT2NUM(VIR_STREAM_EVENT_ERROR));
    rb_define_const(c_stream, "EVENT_HANGUP", INT2NUM(VIR_STREAM_EVENT_HANGUP));

    rb_define_method(c_stream, "send", libvirt_stream_send, 1);
    rb_define_method(c_stream, "recv", libvirt_stream_recv, 2);
    rb_define_method(c_stream, "sendall", libvirt_stream_sendall, -1);
    rb_define_method(c_stream, "recvall", libvirt_stream_recvall, -1);

    rb_define_method(c_stream, "event_add_callback",
                     libvirt_stream_event_add_callback, -1);
    rb_define_method(c_stream, "event_update_callback",
                     libvirt_stream_event_update_callback, 1);
    rb_define_method(c_stream, "event_remove_callback",
                     libvirt_stream_event_remove_callback, 0);
    rb_define_method(c_stream, "finish", libvirt_stream_finish, 0);
    rb_define_method(c_stream, "abort", libvirt_stream_abort, 0);
    rb_define_method(c_stream, "free", libvirt_stream_free, 0);
#endif
}
