#ifndef _ZONES_H_
#define _ZONES_H_

#define MAXZONENAMELEN 	128
#define MAXZONES	1024
#define MAXZONEIDS	3000	

#ifndef _ZONEID_T_DEFINED_
#define _ZONEID_T_DEFINED_
typedef int zoneid_t;
#endif

zoneid_t zone_create(const char *zonename);
int zone_destroy(zoneid_t z);
int zone_enter(zoneid_t z);
int zone_list(zoneid_t *zs, size_t *nzs);
int zone_name(zoneid_t z, char *name, size_t namelen);

#endif
