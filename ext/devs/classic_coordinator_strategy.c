#include <classic_coordinator_strategy.h>

VALUE cDEVSClassicCoordinatorStrategy;

static VALUE handle_init_event(VALUE self, VALUE event);
static VALUE handle_input_event(VALUE self, VALUE event);
static VALUE handle_output_event(VALUE self, VALUE event);
static VALUE handle_internal_event(VALUE self, VALUE event);

void
init_devs_classic_coordinator_strategy() {
    VALUE mod = rb_define_module_under(mDEVSClassic, "CoordinatorStrategy");
    cDEVSClassicCoordinatorStrategy = mod;

    rb_define_method(mod, "handle_init_event", handle_init_event, 1);
    rb_define_method(mod, "handle_input_event", handle_input_event, 1);
    rb_define_method(mod, "handle_output_event", handle_output_event, 1);
    rb_define_method(mod, "handle_internal_event", handle_internal_event, 1);
}

// Handles events of init type (i messages)
//
// @param event [Event] the init event
static VALUE
handle_init_event(VALUE self, VALUE event) {
    VALUE children = rb_iv_get(self, "@children");
    VALUE model = rb_iv_get(self, "@model");

    for (int i = 0; i < RARRAY_LEN(children); i++) {
        VALUE child = rb_ary_entry(children, i);
        rb_funcall(child, rb_intern("dispatch"), 1, event);
    }

    VALUE tl = rb_funcall(self, rb_intern("max_time_last"), 0);
    VALUE tn = rb_funcall(self, rb_intern("min_time_next"), 0);
    rb_iv_set(self, "@time_last", tl);
    rb_iv_set(self, "@time_next", tn);

    // debug "#{model} set tl: #{@time_last}; tn: #{@time_next}"
    DEVS_DEBUG("set tl: %f; tn: %f", NUM2DBL(tl), NUM2DBL(tn));
    return Qnil;
}

// Handles input events (x messages)
//
// @param event [Event] the input event
// @raise [BadSynchronisationError] if the event time isn't in a proper
//   range, e.g isn't between {Coordinator#time_last} and
//   {Coordinator#time_next}
static VALUE
handle_input_event(VALUE self, VALUE event) {
    VALUE model = rb_iv_get(self, "@model");
    VALUE msg = rb_iv_get(event, "@message");
    VALUE port = rb_iv_get(msg, "@port");
    VALUE payload = rb_iv_get(msg, "@payload");
    double time_last = NUM2DBL(rb_iv_get(self, "@time_last"));
    double time_next = NUM2DBL(rb_iv_get(self, "@time_next"));
    double ev_time = NUM2DBL(rb_iv_get(event, "@time"));

    if (ev_time >= time_last && ev_time <= time_next) {
        VALUE ret = rb_funcall(model, rb_intern("each_input_coupling"), 1, port);
        for (int i = 0; i < RARRAY_LEN(ret); i++) {
            VALUE coupling = rb_ary_entry(ret, i);
            VALUE mdl_dst = rb_funcall(coupling, rb_intern("destination"), 0);
            VALUE child = rb_funcall(mdl_dst, rb_intern("processor"), 0);
            VALUE prt_dst = rb_iv_get(coupling, "@destination_port");

            // debug "    #{model} found external input coupling #{coupling}"
            DEVS_DEBUG("found external input coupling");

            VALUE msg2 = rb_funcall(
                cDEVSMessage,
                rb_intern("new"),
                2,
                payload,
                prt_dst
            );
            VALUE ev = rb_funcall(
                cDEVSEvent,
                rb_intern("new"),
                3,
                ID2SYM(rb_intern("input")),
                rb_float_new(ev_time),
                msg2
            );
            rb_funcall(child, rb_intern("dispatch"), 1, ev);
        }

        rb_iv_set(self, "@time_last", rb_float_new(ev_time));
        VALUE tn = rb_funcall(self, rb_intern("min_time_next"), 0);
        rb_iv_set(self, "@time_next", tn);
        //   debug "#{model} time_last: #{@time_last} | time_next: #{@time_next}"
        DEVS_DEBUG("time_last: %f | time_next: %f", ev_time, NUM2DBL(tn));
    } else {
        rb_raise(
            cDEVSBadSynchronisationError,
            "time: %f should be between time_last: %f and time_next: %f",
            ev_time,
            time_last,
            time_next
        );
    }

    return Qnil;
}

// Handles output events (y messages)
//
// @param event [Event] the output event
static VALUE
handle_output_event(VALUE self, VALUE event) {
    VALUE model = rb_iv_get(self, "@model");
    VALUE msg = rb_iv_get(event, "@message");
    VALUE port = rb_iv_get(msg, "@port");
    VALUE payload = rb_iv_get(msg, "@payload");
    VALUE parent = rb_iv_get(self, "@parent");
    VALUE time = rb_iv_get(event, "@time");

    VALUE ret = rb_funcall(model, rb_intern("each_output_coupling"), 1, port);
    for (int i = 0; i < RARRAY_LEN(ret); i++) {
        VALUE coupling = rb_ary_entry(ret, i);
        VALUE prt_dst = rb_iv_get(coupling, "@destination_port");
        // debug "    found external output coupling #{coupling}"
        DEVS_DEBUG("found external output coupling");

        VALUE msg2 = rb_funcall(
            cDEVSMessage,
            rb_intern("new"),
            2,
            payload,
            prt_dst
        );
        VALUE ev = rb_funcall(
            cDEVSEvent,
            rb_intern("new"),
            3,
            ID2SYM(rb_intern("output")),
            time,
            msg2
        );
        rb_funcall(parent, rb_intern("dispatch"), 1, ev);
    }

    ret = rb_funcall(model, rb_intern("each_internal_coupling"), 1, port);
    for (int i = 0; i < RARRAY_LEN(ret); i++) {
        VALUE coupling = rb_ary_entry(ret, i);
        VALUE mdl_dst = rb_funcall(coupling, rb_intern("destination"), 0);
        VALUE child = rb_funcall(mdl_dst, rb_intern("processor"), 0);
        VALUE prt_dst = rb_iv_get(coupling, "@destination_port");

        DEVS_DEBUG("found internal coupling");

        VALUE msg2 = rb_funcall(
            cDEVSMessage,
            rb_intern("new"),
            2,
            payload,
            prt_dst
        );
        VALUE ev = rb_funcall(
            cDEVSEvent,
            rb_intern("new"),
            3,
            ID2SYM(rb_intern("input")),
            time,
            msg2
        );
        rb_funcall(child, rb_intern("dispatch"), 1, ev);
    }

    return Qnil;
}

// Handles star events (* messages)
//
// @param event [Event] the star event
// @raise [BadSynchronisationError] if the event time is not equal to
//   {Coordinator#time_next}
static VALUE
handle_internal_event(VALUE self, VALUE event) {
    double time_next = NUM2DBL(rb_iv_get(self, "@time_next"));
    double ev_time = NUM2DBL(rb_iv_get(event, "@time"));
    VALUE model = rb_iv_get(self, "@model");

    if (ev_time != time_next) {
        rb_raise(
            cDEVSBadSynchronisationError,
            "time: %f should match time_next: %f",
            ev_time,
            time_next
        );
    }

    VALUE children = rb_funcall(self, rb_intern("imminent_children"), 0);
    VALUE children_models = rb_ary_new2(RARRAY_LEN(children));
    for (int i = 0; i < RARRAY_LEN(children); i++) {
        VALUE child = rb_ary_entry(children, i);
        rb_ary_push(children_models, rb_iv_get(child, "@model"));
    }
    VALUE child_model = rb_funcall(model, rb_intern("select"), 1, children_models);
    //   debug "    selected #{child_model} in #{children_models.map(&:name)}"
    int index;
    for (index = 0; index < RARRAY_LEN(children); index++) {
        if (child_model == rb_ary_entry(children_models, index)) {
            break;
        }
    }
    VALUE child = rb_ary_entry(children, index);

    rb_funcall(child, rb_intern("dispatch"), 1, event);

    rb_iv_set(self, "@time_last", rb_float_new(ev_time));
    VALUE tn = rb_funcall(self, rb_intern("min_time_next"), 0);
    rb_iv_set(self, "@time_next", tn);

    DEVS_DEBUG("time_last: %f | time_next: %f", ev_time, NUM2DBL(tn));

    return Qnil;
}
