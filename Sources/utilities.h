#include <m_pd.h>

#define pd_assert(pd_obj, condition, message)                                                      \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            pd_error(pd_obj, message);                                                             \
            return;                                                                                \
        }                                                                                          \
    } while (0)
