#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/tree.h>
#include <sys/rwlock.h>
#include <sys/mount.h>
#include <sys/atomic.h>
#include <sys/syscallargs.h>

//#define _DEBUG_ZONE_	1

#define MAXZONENAMELEN  128
#define MAXZONES        1024
#define MAXZONEIDS     	3000 

int
zone_cmp(struct zone_node *z1, struct zone_node *z2)
{
	return (z1->zone_id < z2->zone_id ? -1 : z1->zone_id > z2->zone_id);
}

zoneid_t zoneid_head = 1;
int current_zones_total = 0;
struct rwlock zone_rwlock = RWLOCK_INITIALIZER("zone_rwlock");
RB_HEAD(zone_rbtree, zone_node) head = RB_INITIALIZER(&head);
RB_GENERATE(zone_rbtree, zone_node, entry, zone_cmp);

void
print_rbtree(void)
{
	printf("______ZONE_RBTREE_____\n");
	struct zone_node *zn;
	RB_FOREACH(zn, zone_rbtree, &head){
		/*printf("Zone_ID: [%d]\n", zn->zone_id);
		printf("Name: [%s]\n", zn->zone_name);
		printf("Processes: [%d]\n", zn->ps_total);
		printf("__________^.^________\n");*/
		printf("[%d]:%s/%d\n", zn->zone_id, zn->zone_name, zn->ps_total);
	}
	return;
}

struct zone_node* 
search_rbtree_name(char* search_name)
{
	struct zone_node  *res, *zn;
	res = NULL;
	RB_FOREACH(zn, zone_rbtree, &head){
		if(strcmp(zn->zone_name, search_name)==0) {
			res = zn;
		}
	}
	return res;
}

struct zone_node* 
search_rbtree_zoneid(zoneid_t search_zoneid)
{
	struct zone_node find, *res;
	find.zone_id = search_zoneid;
	res = RB_FIND(zone_rbtree, &head, &find);	
	return res;
}

int
is_process_root(struct process* ps)
{
	struct ucred *ps_ucred = ps->ps_ucred;
	return (ps_ucred->cr_uid == 0);
}

int
is_process_global(struct process* ps)
{
	#ifdef _DEBUG_ZONE_
	if (ps->ps_zone != NULL) {printf("Zone[%d] already defined! \n", (ps->ps_zone)->zone_id);}
	#endif
	return (ps->ps_zone == NULL);
}

zoneid_t 
generate_zoneid(void)
{
	struct zone_node* res;	
	res = search_rbtree_zoneid(zoneid_head);
	while (1) {
		if (res == NULL) {
			return zoneid_head++;
		} else {
			zoneid_head++;
			res = NULL;
			res = search_rbtree_zoneid(zoneid_head);
		}
	}
}

int
sys_zone_create(struct proc *p, void *v, register_t *retval)
{
	struct sys_zone_create_args /* {
		syscallarg(const char *) zonename;
	} */ *uap = v; 

	int error;
	size_t copied_len;
	struct zone_node *zn;
	struct process *ps = p->p_p;
	char *buffer = malloc(sizeof(char)*MAXZONENAMELEN, M_SUBPROC, M_WAITOK | M_ZERO);

	error = copyinstr(SCARG(uap, zonename), buffer, MAXZONENAMELEN, &copied_len);
	if (error) {
		return (error);
	}

	if (!is_process_global(ps)||!is_process_root(ps)) {
		return (EPERM);
	}

	rw_enter_write(&zone_rwlock);
	// Search by name
	if (search_rbtree_name(buffer) != NULL) {
		rw_exit_write(&zone_rwlock);
		return (EEXIST);
	}

	// Too many zones currently running
	if (current_zones_total >= MAXZONES) {
		rw_exit_write(&zone_rwlock);
		return (ERANGE);
	}

	// Create a zone entry
	zn = malloc(sizeof(struct zone_node), M_SUBPROC, M_WAITOK | M_ZERO);
	zn->zone_id = generate_zoneid();
	zn->zone_name = buffer;
	zn->ps_total = 0;

	// Write to the RB tree
	RB_INSERT(zone_rbtree, &head, zn);
	current_zones_total++;

	// Debugging
	#ifdef _DEBUG_ZONE_
	printf("%s!\n", __func__);
	print_rbtree();
	#endif

	*retval = zn->zone_id;
	rw_exit_write(&zone_rwlock);
	return (0);
}

int
sys_zone_destroy(struct proc *p, void *v, register_t *retval)
{
	struct sys_zone_destroy_args /* {
		syscallarg(zoneid_t) z;
	} */ *uap = v; 

	struct process *ps = p->p_p;
	struct zone_node *search_res;
	
	if (!is_process_global(ps)||!is_process_root(ps)) {
		return (EPERM);
	}

	rw_enter_write(&zone_rwlock);
	// Search and remove, if valid zone
	search_res = search_rbtree_zoneid(SCARG(uap, z));
	if (search_res == NULL) {
		rw_exit_write(&zone_rwlock);
		return (ESRCH);
	} else if (search_res->ps_total > 0) {
		rw_exit_write(&zone_rwlock);
		return (EBUSY);
	} else {
		RB_REMOVE(zone_rbtree, &head, search_res);
		current_zones_total--;
		free(search_res->zone_name, M_SUBPROC, 0);
		free(search_res, M_SUBPROC, sizeof(struct zone_node)); 
	}

	#ifdef _DEBUG_ZONE_
	printf("%s!\n", __func__);
	print_rbtree();
	#endif

	rw_exit_write(&zone_rwlock);
	return (0);
}

int
sys_zone_enter(struct proc *p, void *v, register_t *retval)
{
	struct sys_zone_enter_args /* {
		syscallarg(zoneid_t) z;
	} */ *uap = v;


	struct zone_node *search_zone;
	struct process *ps = p->p_p;

	if (!is_process_global(ps)||!is_process_root(ps)){
		return (EPERM);
	}

	rw_enter_write(&zone_rwlock);
	// Assign zone_id and increment process total
	search_zone = search_rbtree_zoneid(SCARG(uap, z));
	if (search_zone == NULL) {
		rw_exit_write(&zone_rwlock);
		return (ESRCH);
	} else {
		ps->ps_zone = search_zone;
		atomic_inc_int(&(search_zone->ps_total));
	}

	#ifdef _DEBUG_ZONE_
	printf("%s!\n", __func__);
	print_rbtree();
	#endif
	
	rw_exit_write(&zone_rwlock);
	return (0);
}

int
sys_zone_list(struct proc *p, void *v, register_t *retval)
{
	struct sys_zone_list_args /* {
		syscallarg(zoneid_t *) zs;
		syscallarg(size_t *) nzs;
	} */ *uap = v;

	int error;
	zoneid_t* zoneid_array;
	struct process *ps = p->p_p;
	size_t kernel_zones;
	size_t user_nzs;

	error = copyin(SCARG(uap, nzs), &user_nzs, sizeof(size_t*));
	if (error){
		return (error);
	}

	rw_enter_read(&zone_rwlock);
	if ((!is_process_global(ps) && (user_nzs == 0) )
		|| (is_process_global(ps) && (user_nzs < current_zones_total) )) {
		rw_exit_read(&zone_rwlock);
		return (ERANGE);
	}

	if (!is_process_global(ps)) {
		zoneid_array = malloc(sizeof(zoneid_t), M_SUBPROC, M_WAITOK | M_ZERO);
		zoneid_array[0] = (ps->ps_zone)->zone_id;
		kernel_zones = 1;
		error = copyout(zoneid_array, SCARG(uap, zs), sizeof(zoneid_t));
		free(zoneid_array, M_SUBPROC, sizeof(zoneid_t));
	} else {
		struct zone_node *zn;
		int array_counter = 0;
		kernel_zones = current_zones_total;
		zoneid_array = malloc(sizeof(zoneid_t)*current_zones_total, M_SUBPROC, M_WAITOK | M_ZERO);

		RB_FOREACH(zn, zone_rbtree, &head){
			zoneid_array[array_counter++] = zn->zone_id;
		}
		error = copyout(zoneid_array, SCARG(uap, zs), sizeof(zoneid_t)*kernel_zones);
		free(zoneid_array, M_SUBPROC, sizeof(zoneid_t)*current_zones_total);
	}

	// Error for copying out the zoneid_array
	if (error){
		rw_exit_read(&zone_rwlock);
		return (error);
	}

	error = copyout(&kernel_zones, SCARG(uap, nzs), sizeof(size_t*));
	if (error){
		rw_exit_read(&zone_rwlock);
		return (error);
	}
	
	// Debugging
	#ifdef _DEBUG_ZONE_
	printf("%s!\n", __func__);
	print_rbtree();
	#endif

	rw_exit_read(&zone_rwlock);
	return (0);
}

int
sys_zone_name(struct proc *p, void *v, register_t *retval)
{
	struct sys_zone_name_args /* {
		syscallarg(zoneid_t) Z;
		syscallarg(char *) name;
		syscallarg(size_t) namelen;
	} */ *uap = v; 

	int error;
	size_t copied_out_len;
	struct zone_node *zn;
	struct process *ps = p->p_p;

	// Search tree by id
	rw_enter_read(&zone_rwlock);
	zn = search_rbtree_zoneid(SCARG(uap, Z));
	if (zn == NULL) {
		rw_exit_read(&zone_rwlock);
		return (ESRCH);
	} else if (!is_process_global(ps)){
		if ((ps->ps_zone)->zone_id != zn->zone_id){
			rw_exit_read(&zone_rwlock);
			return (ESRCH);
		}
	} 
	error = copyoutstr(zn->zone_name, SCARG(uap, name), SCARG(uap, namelen), &copied_out_len);
	if (error) {
		rw_exit_read(&zone_rwlock);
		return (error);
	}
	
	#ifdef _DEBUG_ZONE_
	printf("%s - [%d]:%s/%d\n", __func__, 
		SCARG(uap, Z), zn->zone_name, zn->ps_total);
	#endif

	rw_exit_read(&zone_rwlock);
	return (0);
}



