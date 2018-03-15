var pmdk_types = [
        /* libpmemobj.h */
        "PMEMobjpool",  "PMEMmutex", "PMEMrwlock", "PMEMcond", "PMEMoid",
        "pmemobj_constr", "tx_lock",
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
        /* librpmemcto.h */
        "PMEMctopool",
        /* other headers */
        "mode_t", "timespec"
];

/* ambiguous types can also be e.g. define names */
var pmdk_ambiguous_types = [
        /* libpmemobj.h */
        "POBJ_LIST_ENTRY", "POBJ_LIST_HEAD", "TOID"
];

/*
 * pmdk_format_type -- format an element
 */
function pmdk_format_type(obj) {
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
 * pmdk_format -- filter elements requiring formatting
 */
function pmdk_format() {
        var word = $(this).text();
        if (pmdk_types.indexOf(word) != -1)
                pmdk_format_type(this);
        else if (pmdk_ambiguous_types.indexOf(word) != -1) {
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

                pmdk_format_type(this);
        }
}

/* trigger PMDK types formatting */
$(document).ready(function() {
        $("code span.n").each(pmdk_format);
        $("code.highlighter-rouge").each(pmdk_format);
});
