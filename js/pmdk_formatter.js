var nvml_types = [
        /* libpmemobj.h */
        "PMEMobjpool",  "PMEMmutex", "PMEMrwlock", "PMEMcond", "PMEMoid",
        "pmemobj_constr", "tx_lock", "pmemobj_constr",
        /* libpmemblk.h */
        "PMEMblkpool",
        /* libpmemlog.h */
        "PMEMlogpool",
        /* libvmem.h */
        "VMEM",
        /* libpmempool.h */
        "PMEMpoolcheck",
        /* librpmem.h */
        "RPMEMpool",
        /* other headers */
        "mode_t", "timespec"
];

/* ambiguous types can also be e.g. define names */
var nvml_ambiguous_types = [
        /* libpmemobj.h */
        "POBJ_LIST_ENTRY", "POBJ_LIST_HEAD", "TOID"
];

/*
 * nvml_format_type -- format an element
 */
function nvml_format_type(obj) {
        /* inject span into inline code tag */
        var tag = $(obj).prop("tagName");
        if (tag == "CODE") {
                var text = $(obj).text();
                $(obj).html("<span>" + text + "</span>").addClass("highlight");
                obj = $(obj).children("span");
        }

        $(obj).addClass("kt").removeClass("n");
}

/*
 * nvml_format -- filter elements requiring formatting
 */
function nvml_format() {
        var word = $(this).text();
        if (nvml_types.indexOf(word) != -1)
                nvml_format_type(this);
        else if (nvml_ambiguous_types.indexOf(word) != -1) {
                /*
                 * if succeeding tag is a span with "(" it is a name of a
                 * function / define but not a type
                 */
                var next = $(this).next("span");
                if (next.size() == 1) {
                        var next_word = $(next).text();
                        if (next_word == "(")
                                return;
                }

                nvml_format_type(this);
        }
}

/* trigger NVML types formatting */
$(document).ready(function() {
        $("code span.n").each(nvml_format);
        $("code.highlighter-rouge").each(nvml_format);
});
