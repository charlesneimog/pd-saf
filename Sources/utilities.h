#include <m_pd.h>
#include <math.h>

#define pd_assert(pd_obj, condition, message)                                                      \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            pd_error(pd_obj, message);                                                             \
            return;                                                                                \
        }                                                                                          \
    } while (0)

int get_ambisonic_order(int nIn) {
    int order = (int)(sqrt(nIn) - 1);
    return (order + 1) * (order + 1) == nIn ? order : -1; // Return -1 if invalid
}
