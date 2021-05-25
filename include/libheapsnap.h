#ifndef __LIB_HEAPSNAP_H__
#define __LIB_HEAPSNAP_H__

#ifdef __cplusplus
extern "C" {
#endif

void heapsnap_init();
void heapsnap_save(void);

#ifdef __cplusplus
}
#endif

#endif //__LIB_HEAPSNAP_H__