#include <m_pd.h>
#include <s_stuff.h>
#include <m_imp.h>

static t_class *saf_libclass;

typedef struct _saf {
    t_object x_obj;
} t_saf;

static void *saf_new(void) {
    return pd_new(saf_libclass);
}

void saf_setup(void) {
    int major, minor, micro;
    sys_getversion(&major, &minor, &micro);
    if (major < 0 && minor < 54) {
        return;
    }

    logpost(0, 2, "\n[saf] pd-saf by Charles K. Neimog\n");

    saf_libclass =
        class_new(gensym("saf"), (t_newmethod)saf_new, 0, sizeof(t_saf), CLASS_NOINLET, 0);

    t_canvas *cnv = canvas_getcurrent();
    const char *requiredLibs[] = {"pdlua"};

    for (size_t i = 0; i < 1; ++i) {
        int result = sys_load_lib(cnv, requiredLibs[i]);
        if (!result) {
            pd_error(NULL,
                     "[pd-saf] %s not installed, Gui objects will not work! Please, install pdlua!",
                     requiredLibs[i]);
            post("[pd-saf]    Go to Help -> Find Externals");
            post("[pd-saf]    Search for pdlua and install it");
        }
    }

    // add to the search path
    const char *safpath = saf_libclass->c_externdir->s_name;
    STUFF->st_searchpath = namelist_append(STUFF->st_searchpath, safpath, 0);
}
