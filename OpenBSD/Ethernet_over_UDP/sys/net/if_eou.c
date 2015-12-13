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
#include <sys/time.h>
#include <sys/syscallargs.h>

#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>

#include <crypto/siphash.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/in_pcb.h>

#include <net/if_eou.h>

#define	_DEBUG_EOU_	
#define _EOU_T_VER_	1

/* Functions for the pseudo-device and ifmedia API */
void	eouattach(int);
int eou_clone_create(struct if_clone *, int);
int eou_clone_destroy(struct ifnet *);
void 	eoustart(struct ifnet *);
int eouioctl(struct ifnet *, u_long, caddr_t);
int eou_media_change(struct ifnet *);
void	eou_media_status(struct ifnet *, struct ifmediareq *);

/* Functions for softc handling */
int eou_softc_config(struct ifnet *, struct sockaddr *, struct sockaddr *);
int eou_softc_cleanup(struct ifnet *);
int eou_softc_setup(struct ifnet *);
int eou_softc_ping(struct ifnet *);
int eou_softc_send(struct ifnet *, struct mbuf *);
void	eou_softc_heartbeat(void *);
void 	eou_softc_heartattack(void *);

/* Handles tunnel operations */
int eou_tunnel_create(struct ifnet *, struct sockaddr_in *, struct sockaddr_in *, struct eou_tunnel *);
int eou_tunnel_destroy(struct eou_tunnel *);
void 	eou_tunnel_upcall(struct socket *, caddr_t, int);

/* Tree comparison functions */
int cmp_addr(struct sockaddr_storage*, struct sockaddr_storage*);
int eou_id_cmp(struct eou_softc *, struct eou_softc *);
int tun_id_cmp(struct eou_tunnel *, struct eou_tunnel *);

/* Hashing for the tunnels */
uint32_t generate_tun_id(struct sockaddr_storage, struct sockaddr_storage, in_port_t);

/* Searching tree functions */ 
struct eou_softc* search_eou_softc(struct sockaddr_storage, struct sockaddr_storage, in_port_t, u_int32_t);
struct eou_tunnel* search_eou_tunnel(struct sockaddr_storage, struct sockaddr_storage, in_port_t);

/* Debugging Functions */
void print_network_address(struct sockaddr_storage, struct sockaddr_storage, in_port_t);
void print_eou_tunnel_tree(void);
void print_eou_softc_tree(void);

/* RBtree to store the pseudo-device tunnels and softc*/
RB_HEAD(eou_tunnel_tree, eou_tunnel) eou_tunnel_head = RB_INITIALIZER(&eou_tunnel_head);
RB_GENERATE(eou_tunnel_tree, eou_tunnel, entry, tun_id_cmp);

RB_HEAD(eou_softc_tree, eou_softc) eou_softc_head = RB_INITIALIZER(&eou_softc_head); 
RB_GENERATE(eou_softc_tree, eou_softc, entry, eou_id_cmp);

/* A rwlock for each RBtree */
struct rwlock eou_softc_lock  = RWLOCK_INITIALIZER("eou_softc_lock");
struct rwlock eou_tunnel_lock = RWLOCK_INITIALIZER("eou_tunnel_lock"); 

/* Global key for Server */
SIPHASH_KEY master_key;

/* Pseudo-device attacher */
struct if_clone eou_cloner = 
	IF_CLONE_INITIALIZER("eou", eou_clone_create, eou_clone_destroy);

int
eou_id_cmp(struct eou_softc *s1, struct eou_softc *s2)
{
	return(s1->tun_id < s2->tun_id ? -1 : s1->tun_id > s2->tun_id);
}

int
tun_id_cmp(struct eou_tunnel *t1, struct eou_tunnel *t2)
{
	return(t1->tun_id < t2->tun_id ? -1 : t1->tun_id > t2->tun_id);
}

int 
cmp_addr(struct sockaddr_storage *addr1, struct sockaddr_storage *addr2)
{
	struct sockaddr_in addr1_4;
	struct sockaddr_in addr2_4;
	
	memcpy(&addr1_4, addr1, addr1->ss_len);
	memcpy(&addr2_4, addr2, addr2->ss_len);

	if (addr1_4.sin_family == addr2_4.sin_family && 
		addr1_4.sin_port == addr2_4.sin_port &&
		addr1_4.sin_addr.s_addr == addr2_4.sin_addr.s_addr) {
		return 0;

	}
	return 1;
}

struct eou_tunnel*
search_eou_tunnel(struct sockaddr_storage src, struct sockaddr_storage dst, in_port_t dst_port)
{	
	struct eou_tunnel *res, *tun_node;
	uint32_t search_hash = generate_tun_id(src, dst, dst_port);

	/* Collisions can occur */
	rw_enter_read(&eou_tunnel_lock);
	RB_FOREACH(tun_node, eou_tunnel_tree, &eou_tunnel_head){
		if ( ( (tun_node->tun_id) == search_hash) && 
			( cmp_addr(&tun_node->tun_src,&src) == 0) && 
			( cmp_addr(&tun_node->tun_dst,&dst) == 0) &&
			( (tun_node->tun_dstport) == dst_port)
			) {
			res = tun_node;
			break;
		}
	}
	rw_exit_read(&eou_tunnel_lock);
	return res;
}

struct eou_softc*
search_eou_softc(struct sockaddr_storage src, struct sockaddr_storage dst, in_port_t dst_port, u_int32_t vnet_id)
{
	struct eou_softc *res, *softc_node;
	rw_enter_read(&eou_softc_lock);
	RB_FOREACH(softc_node, eou_softc_tree, &eou_softc_head){
		if ( (cmp_addr(&softc_node->sc_src, &src) == 0) &&
			(cmp_addr(&softc_node->sc_dst, &dst) == 0) &&
			(softc_node->sc_dstport = dst_port) &&
			(softc_node->sc_vnetid == vnet_id)) {
			res = softc_node;
			break;
		}
	}
	rw_exit_read(&eou_softc_lock);
	return res;
}

void
print_eou_tunnel_tree(void)
{
	struct eou_tunnel *tun_node;	
	rw_enter_read(&eou_tunnel_lock);
	RB_FOREACH(tun_node, eou_tunnel_tree, &eou_tunnel_head){
		printf("Total [%d] @ ", tun_node->eou_nets);
		print_network_address(tun_node->tun_src, 
			tun_node->tun_dst, tun_node->tun_dstport);
	}
	rw_exit_read(&eou_tunnel_lock);		
}


void
print_eou_softc_tree(void)
{
	struct eou_softc *softc_node;
	rw_enter_read(&eou_softc_lock);
	RB_FOREACH(softc_node, eou_softc_tree, &eou_softc_head){
		printf("NetID [%d] @ ", softc_node->sc_vnetid);
		print_network_address(softc_node->sc_src, 
			softc_node->sc_dst, softc_node->sc_dstport);
	}
	rw_exit_read(&eou_softc_lock);
}

/* Generate a hash for every eou_tunnel */
uint32_t 
generate_tun_id(struct sockaddr_storage src, struct sockaddr_storage dst, in_port_t dst_port)
{
	struct sockaddr_in src4;
	struct sockaddr_in dst4;
	
	memcpy(&src4, &src, src.ss_len);
	memcpy(&dst4, &dst, dst.ss_len);

	return (dst4.sin_addr.s_addr*13 - src4.sin_addr.s_addr*17) + dst_port*7;
}


uint32_t 
generate_softc_id(struct sockaddr_storage src, struct sockaddr_storage dst, in_port_t dst_port, u_int32_t vnetid)
{
	struct sockaddr_in src4;
	struct sockaddr_in dst4;
	
	memcpy(&src4, &src, src.ss_len);
	memcpy(&dst4, &dst, dst.ss_len);

	return (dst4.sin_addr.s_addr*13 - src4.sin_addr.s_addr*17) + dst_port*7 + 11*vnetid;
}

void
print_network_address(struct sockaddr_storage src, struct sockaddr_storage dst, in_port_t dst_port)
{
	struct sockaddr_in src4;
	struct sockaddr_in dst4;
	
	memcpy(&src4, &src, src.ss_len);
	memcpy(&dst4, &dst, dst.ss_len);

	char src_buffer[INET_ADDRSTRLEN];
	char dst_buffer[INET_ADDRSTRLEN];
	int src_len, dst_len;
	src_len = dst_len = INET_ADDRSTRLEN;

	inet_ntop(AF_INET, &(src4.sin_addr), src_buffer, src_len);
	inet_ntop(AF_INET, &(dst4.sin_addr), dst_buffer, dst_len);

	printf("[%s] -> [%s|%hu]\n", src_buffer, dst_buffer, htons(dst_port));
}


int
eou_media_change(struct ifnet *ifp)
{
	// TODO if brought up, then setup for ping
	printf("__Media Changed__\n");
	return(0);
}

void
eou_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	//imr->ifm_active = IFM_ETHER | IFM_AUTO;
	imr->ifm_active = IFM_ETHER;
	//imr->ifm_status = IFM_AVALID | IFM_ACTIVE;

	if (ifp->if_link_state == LINK_STATE_UP) {
		imr->ifm_status = IFM_ACTIVE;
	} else {
		imr->ifm_status = IFM_AVALID;
	}
}

void
eouattach(int count)
{
	/* Setup the master key */
	char secret_string[] = "comp3301comp7308";
	memcpy(&master_key, secret_string, sizeof(secret_string)); 	
	
	if_clone_attach(&eou_cloner);
}

int 
eou_clone_create(struct if_clone *ifc, int unit)
{
	#ifdef _DEBUG_EOU_
	//printf("EOU [Create]\n");
	#endif

	struct ifnet		*ifp;
	struct eou_softc	*sc;

	if ((sc = malloc(sizeof(*sc), 
		M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL){
		return (ENOMEM);
	}

	sc->sc_vnetid = 0;

	ifp = &sc->sc_ac.ac_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "eou%d", unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ether_fakeaddr(ifp);

	ifp->if_softc = sc;
	ifp->if_ioctl = eouioctl;
	ifp->if_start = eoustart;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_capabilities = IFCAP_VLAN_MTU;

	ifmedia_init(&sc->sc_media, 0, eou_media_change, eou_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0 , NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);
	
	sc->sc_ifp = ifp;
	timeout_set(&sc->timer_heartbeat, eou_softc_heartbeat, ifp);
	timeout_set(&sc->timer_heartattack, eou_softc_heartattack, ifp);

	if_attach(ifp);
	ether_ifattach(ifp);
	return (0);
}

int
eou_clone_destroy(struct ifnet *ifp)
{
	#ifdef _DEBUG_EOU_
	printf("EOU [Destroy]\n");
	#endif

	struct eou_softc *sc = (struct eou_softc*) ifp->if_softc;
	eou_softc_cleanup(ifp);

	ifmedia_delete_instance(&sc->sc_media, IFM_INST_ANY);
	ether_ifdetach(ifp);
	if_detach(ifp);
	free(sc, M_DEVBUF, sizeof(*sc));
	return (0);
}

/* ARGSUSED?????????? wut*/
int 
eouioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct eou_softc	*sc = (struct eou_softc *)ifp->if_softc;
	struct ifaddr		*ifa = (struct ifaddr *)data;
	struct ifreq		*ifr = (struct ifreq *)data;
	struct if_laddrreq	*lifr = (struct if_laddrreq *)data;
	struct proc		*p = curproc;
	int			 error = 0, s;

	switch(cmd) {
	case SIOCSIFADDR:
		printf("SIOCSIFADDR\n");
		ifp->if_flags |= IFF_UP;
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
		/* FALLTHROUGH */	
	case SIOCSIFFLAGS:
		printf("SIOCSIFFLAGS\n");
		if (ifp->if_flags & IFF_UP) {
			eou_softc_setup(ifp);
		} else {
			timeout_del(&sc->timer_heartbeat);
			timeout_del(&sc->timer_heartattack);
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;
	case SIOCSLIFPHYADDR: /* Set address */
		printf("SIOCSLIFPHYADDR\n");
		if ((error = suser(p,0)) != 0)
			break;
		s = splnet();
		error = eou_softc_config(ifp, 
			(struct sockaddr *)&lifr->addr,
			(struct sockaddr *)&lifr->dstaddr);

		splx(s);
		break;
	case SIOCGLIFPHYADDR: /* Get address */
		if (sc->sc_dst.ss_family == AF_UNSPEC) {
			error = EADDRNOTAVAIL;
			break;
		}
		
		printf("SIOCGLIFPHYADDR\n");
		bzero(&lifr->addr, sizeof(lifr->addr));
		bzero(&lifr->dstaddr, sizeof(lifr->dstaddr));
		memcpy(&lifr->addr, &sc->sc_src, sc->sc_src.ss_len);
		memcpy(&lifr->dstaddr, &sc->sc_dst, sc->sc_dst.ss_len);
		break;
	case SIOCDIFPHYADDR: /* Delete address */
		printf("SIOCDIFPHYADDR\n");
		ifp->if_flags &= ~IFF_RUNNING;
		timeout_del(&sc->timer_heartbeat);
		timeout_del(&sc->timer_heartattack);
		bzero(&sc->sc_src, sizeof(sc->sc_src));
		bzero(&sc->sc_dst, sizeof(sc->sc_dst));
		ifp->if_link_state = LINK_STATE_DOWN;
		if_link_state_change(ifp);
		break;
	case SIOCSVNETID: /* Set VNETID */
		if ((error = suser(p, 0)) != 0)
			break;
		if (ifr->ifr_vnetid < 0 || ifr->ifr_vnetid > 0x00ffffff) {
			error = EINVAL;
			break;
		}
		
		s = splnet();
		sc->sc_vnetid = (u_int32_t)ifr->ifr_vnetid;
		ifp->if_flags &= ~IFF_RUNNING;
		error = eou_softc_config(ifp, NULL, NULL); 
		splx(s);
		break;
	case SIOCGVNETID: /* Get VNETID */
		ifr->ifr_vnetid = (int)sc->sc_vnetid;
		break;
	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
	}

	return (error);
}

int
eou_softc_config(struct ifnet* ifp, struct sockaddr *src, struct sockaddr *dst){

	struct eou_softc	*sc = (struct eou_softc *)ifp->if_softc;
	struct eou_tunnel	*eou_tun;
	struct sockaddr_in	*src4, *dst4;
	int error = 0; 

	if (src != NULL && dst != NULL) {
		/* XXX inet6 is not supported */
		if (src->sa_family != AF_INET || dst->sa_family != AF_INET)
			return (EAFNOSUPPORT);
	
		src4 = satosin(src);
		dst4 = satosin(dst);

		if (src4->sin_len != sizeof(*src4) || dst4->sin_len != sizeof(*dst4))
			return (EINVAL);

		if (dst4->sin_port) {
			sc->sc_dstport = dst4->sin_port;	
		} else {
			sc->sc_dstport = htons(DEFAULT_SERVER_PORT);
		}

		struct sockaddr_storage s_src, s_dst;
		memcpy(&s_src, src, src->sa_len);
		memcpy(&s_dst, dst, dst->sa_len);

		struct eou_softc *res_softc = search_eou_softc(s_src, s_dst, sc->sc_dstport, sc->sc_vnetid);

		if (res_softc == NULL) {
			eou_tun = search_eou_tunnel(s_src, s_dst, sc->sc_dstport);			
		} else {
			return (EINVAL);
		}
		
		if (eou_tun == NULL){
			eou_tun = malloc(sizeof(struct eou_tunnel), M_SUBPROC, M_WAITOK | M_ZERO);
			error = eou_tunnel_create(ifp, src4, dst4, eou_tun);
			if (error) {
				return (error);
			}
		}

		/* Copy the address from the structs */
		memcpy(&sc->sc_src, src, src->sa_len);
		memcpy(&sc->sc_dst, dst, dst->sa_len);
		sc->tun_id = generate_softc_id(sc->sc_src, sc->sc_dst, sc->sc_dstport, sc->sc_vnetid);
		sc->sc_send = eou_tun->so_connect;
		eou_tun->eou_nets++;	
		
		/* Insert the SoftC interface into the RBtree*/
		rw_enter_write(&eou_softc_lock);
		RB_INSERT(eou_softc_tree, &eou_softc_head, ifp->if_softc);
		ifp->if_flags |= IFF_RUNNING;
		rw_exit_write(&eou_softc_lock);

		print_eou_softc_tree();
		print_eou_tunnel_tree();
		
		/* Start the ping */
		//eou_softc_setup(ifp);
	}
	return (error);
}


int
eou_tunnel_create(struct ifnet *ifp, struct sockaddr_in *src4, struct sockaddr_in *dst4, struct eou_tunnel *tun)
{
	printf(" __ Creating tunnel __ \n");

	struct eou_softc	*sc = (struct eou_softc *)ifp->if_softc;
	
	struct socket 		*so_bind;
	struct mbuf		*m_src, *mopt;
	struct sockaddr_in 	*nam_src;
	
	//struct socket		*so_connect;
	struct mbuf		*m_dst;
	struct sockaddr_in	*nam_dst;

	struct proc		*p = curproc;
	int 			error = 0;

	/* Bind local end of tunnel to receive data */
	socreate(AF_INET, &so_bind, SOCK_DGRAM, 0);
		
	MGET(mopt, M_WAIT, MT_SOOPTS);
	mopt->m_len = sizeof(int);
	error = sosetopt(so_bind, SOL_SOCKET, SO_REUSEADDR | SO_BINDANY, mopt);
	//m_freem(mopt);
	if(error){
		return (error);
	}

	MGET(m_src, M_WAIT, MT_SONAME);
	nam_src = mtod(m_src, struct sockaddr_in*);
	nam_src->sin_len = m_src->m_len = sizeof(struct sockaddr_in);
	memcpy(nam_src, src4, src4->sin_len);
	nam_src->sin_family = AF_INET;
	nam_src->sin_port = htons(0); 

	error = sobind(so_bind, m_src, p);
	m_freem(m_src);
	if (error) {
		return (error);
	}

	#ifdef _DEBUG_EOU_
	printf("Tunnel bound\n");
	#endif
	
	/* Connect to other end of tunnel to send data */
	MGET(m_dst, M_WAIT, MT_SONAME);
	nam_dst = mtod(m_dst, struct sockaddr_in*);
	nam_dst->sin_len = m_dst->m_len = sizeof(struct sockaddr_in);
	memcpy(nam_dst, dst4, dst4->sin_len);
	nam_dst->sin_family = AF_INET;
	nam_dst->sin_port = sc->sc_dstport;
	error = soconnect(so_bind, m_dst);
	//m_freem(m_dst);
	if (error){
		return (error);
	}

	printf("Tunnel connected to port [%hu]\n", htons(sc->sc_dstport));

	so_bind->so_upcallarg = (caddr_t)tun;
	so_bind->so_upcall = eou_tunnel_upcall;
	
	/* Set eou_tunnel information */ 	
	memcpy(&tun->tun_src, src4, src4->sin_len);
	memcpy(&tun->tun_dst, dst4, dst4->sin_len);
	tun->tun_dstport = sc->sc_dstport;
	tun->tun_id = generate_tun_id(tun->tun_src, tun->tun_dst, tun->tun_dstport);
	tun->so_connect = so_bind;

	/* Insert the tunnel to RB tree */
	rw_enter_write(&eou_tunnel_lock);
	RB_INSERT(eou_tunnel_tree, &eou_tunnel_head, tun);
	rw_exit_write(&eou_tunnel_lock);
	
	return (error);
}

int
eou_tunnel_destroy(struct eou_tunnel *eou_tun)
{
	if (eou_tun != NULL) {
		//soclose(eou_tun->so_connect);
	}
	return (0);
}

int
eou_softc_setup(struct ifnet *ifp)
{
	printf("__Setup Heartbeat__\n");
	struct eou_softc *sc = (struct eou_softc *) ifp->if_softc;
	int error = 0;

	eou_softc_ping(ifp);
	timeout_add_sec(&sc->timer_heartbeat, PING_HEARTBEAT); 
	timeout_add_sec(&sc->timer_heartattack, PING_HEARTATTACK);

	return (error);
}
int
eou_softc_cleanup(struct ifnet *ifp)
{
	printf("__EOU [Softc Cleanup]__\n");

	struct eou_softc *sc = (struct eou_softc *) ifp->if_softc;
	struct eou_tunnel *tun = search_eou_tunnel(sc->sc_src, sc->sc_dst, sc->sc_dstport);
	
	//bzero(&sc->sc_src, sizeof(sc->sc_src));
	//bzero(&sc->sc_dst, sizeof(sc->sc_dst));

	rw_enter_write(&eou_softc_lock);
	RB_REMOVE(eou_softc_tree, &eou_softc_head, ifp->if_softc);
	rw_exit_write(&eou_softc_lock);

	ifp->if_link_state = LINK_STATE_DOWN;
	if_link_state_change(ifp);

	timeout_del(&sc->timer_heartbeat);
	timeout_del(&sc->timer_heartattack);
	
	if (tun != NULL) {
		tun->eou_nets--;
		if (tun->eou_nets == 0) {
			eou_tunnel_destroy(tun);
		}
	}

	return (0);
}
int 
eou_softc_ping(struct ifnet *ifp)
{
	//printf("__Ping__\n");

	struct eou_softc	*sc = (struct eou_softc *)ifp->if_softc;
	struct eou_pingpong *msg_ping = malloc(sizeof(struct eou_pingpong), M_SUBPROC, M_WAITOK | M_ZERO);
	struct eou_header *msg_hdr = &(msg_ping->hdr);
	int error = 0;

	msg_hdr->eou_network = htonl(sc->sc_vnetid);
	msg_hdr->eou_type = htons(EOU_T_PING);;
	msg_ping->utime = htobe64(time_second);
	arc4random_buf(msg_ping->random, sizeof(msg_ping->random));
		
	//printf("Key [%016llX | %016llX]\n",  master_key.k0, master_key.k1);
	SIPHASH_CTX ctx;
	SipHash24_Init(&ctx, &master_key);
	SipHash24_Update(&ctx, &msg_hdr->eou_network, 	sizeof(msg_hdr->eou_network));
	SipHash24_Update(&ctx, &msg_ping->utime, 	sizeof(msg_ping->utime));
	SipHash24_Update(&ctx,  msg_ping->random, 	sizeof(msg_ping->random));
	SipHash24_Final(msg_ping->mac, &ctx);

	struct mbuf *msg_send;
	MGETHDR(msg_send, M_WAIT, MT_DATA);
	msg_send->m_len = 0;
	msg_send->m_pkthdr.len = 0;
	m_copyback(msg_send, 0, sizeof(struct eou_pingpong), (caddr_t)msg_ping, M_WAIT); 
	error = sosend(sc->sc_send, NULL, NULL, msg_send, NULL, 0);

	printf("VnetID [%d] >p> ", sc->sc_vnetid);
	print_network_address(sc->sc_src, sc->sc_dst, sc->sc_dstport);
	
	return (error);
}

void 
eoustart(struct ifnet *ifp)
{
	#ifdef _DEBUG_EOU_
	printf("EOU [Start]\n");
	#endif

	struct mbuf		*m;
	int			 s;

	for (;;) {
		s = splnet();
		IFQ_DEQUEUE(&ifp->if_snd, m);
		splx(s);
		
		if (m == NULL){
			return;
		}
		ifp->if_opackets++;
		eou_softc_send(ifp, m);
	}

}
int
eou_softc_send(struct ifnet *ifp, struct mbuf* if_m)
{
	printf("__Sending by tunnel__\n");

	struct eou_softc	*sc = (struct eou_softc *)ifp->if_softc;
	int error = 0;
	
	if (ifp->if_link_state != LINK_STATE_UP) {
		printf("Cannot send, link staten not UP\n");
	}

	struct eou_header *eouh;

	M_PREPEND(if_m, sizeof(struct eou_header), M_WAIT);
	if (if_m == NULL) {
		return (ENOMEM);
	} 

	eouh = mtod(if_m, struct eou_header *);
	eouh->eou_network = htonl(sc->sc_vnetid);
	eouh->eou_type = htons(EOU_T_DATA);

	error = sosend(sc->sc_send, NULL, NULL, if_m, NULL, MSG_DONTWAIT);

	return (error);
}


void
eou_tunnel_upcall(struct socket *so, caddr_t upcallarg, int waitflag)
{
	printf("__Upcalled");
	struct eou_tunnel 	*tun = (struct eou_tunnel *)upcallarg;
	int error = 0, flagsp;
	flagsp = MSG_DONTWAIT;
	struct mbuf	*mp0;
	struct uio auio;

	if (so == NULL || (so->so_state & SS_ISCONNECTING)) {
		printf("\n Socket not ready \n");
		return;
	}

	error = soreceive(so, NULL, &auio, &mp0, NULL, &flagsp, 0);
	printf(" [%d]__\n", mp0->m_pkthdr.len);

	if (mp0->m_pkthdr.len < 6) {
		printf("Rejected packet\n");	
		return;
	}

	struct eou_header *eou_rep_hdr;
	m_copydata(mp0, 0, sizeof(struct eou_header), (caddr_t)eou_rep_hdr);

	printf("After copy[%d]\n", mp0->m_pkthdr.len);

	uint32_t eou_netid 	= ntohl(eou_rep_hdr->eou_network);
	uint16_t eou_res_type 	= ntohs(eou_rep_hdr->eou_type); 

	if (eou_res_type != EOU_T_PING && eou_res_type != EOU_T_PONG && eou_res_type != EOU_T_DATA) {
		printf("Packet rejected\n");
		return;
	}

	printf("Checking type [%x]|[%x]\n", eou_res_type, eou_netid);
	//print_network_address(tun->tun_src, tun->tun_dst, tun->tun_dstport);
	//print_eou_softc_tree();

	struct eou_softc *res_softc;
	struct eou_softc *softc_node;
	rw_enter_read(&eou_softc_lock);
	RB_FOREACH(softc_node, eou_softc_tree, &eou_softc_head){
		if ( (cmp_addr(&softc_node->sc_src, &tun->tun_src) == 0) &&
			(cmp_addr(&softc_node->sc_dst, &tun->tun_dst) == 0) &&
			(softc_node->sc_dstport = tun->tun_dstport) &&
			(softc_node->sc_vnetid == eou_netid)) {
			res_softc = softc_node;
			break;
		}
	}
	rw_exit_read(&eou_softc_lock);


	if (res_softc != NULL) {
		printf("Found VNetID[%d]\n", res_softc->sc_vnetid);
	}	

	switch(eou_res_type) {
		case EOU_T_PING:
			printf("Please disable loopback\n");
			break;

		case EOU_T_PONG:
			printf("__PONG__\n");
			if (res_softc != NULL) {				
				//printf("_____ CHECKING 1 _____\n");
				/*struct eou_pingpong *eou_rep_pong;
				m_copydata(mp0, 0, sizeof(struct eou_pingpong), (caddr_t)eou_rep_pong);

				uint64_t packet_time, t = time_second;
				packet_time = betoh64(eou_rep_pong->utime);
				if (packet_time > t + 30 || packet_time < t - 30) {
					printf("Packet out of time window\n");
					return;
				}*/
				/*
				uint8_t check_mac[8];
				SIPHASH_CTX ctx;
				SipHash24_Init(&ctx, &master_key);
				SipHash24_Update(&ctx, &eou_netid, sizeof(eou_netid));
				SipHash24_Update(&ctx, &eou_rep_pong->utime, sizeof(&eou_rep_pong->utime));
				SipHash24_Update(&ctx,  eou_rep_pong->random, sizeof(eou_rep_pong->random));
				SipHash24_Final(check_mac, &ctx);
			
				if (memcmp(check_mac, eou_rep_pong, sizeof(check_mac)) != 0) {
					printf("Packet MAC incorrect\n");
					return;
				}*/

				//printf("_____ CHECKING _____\n");
					
				int s;
				s = splnet();
				if ((res_softc->sc_ifp)->if_link_state != LINK_STATE_UP) {
					printf("Bringing up link state\n");
					if (res_softc->sc_ifp != NULL) {
						(res_softc->sc_ifp)->if_link_state = LINK_STATE_UP;
						if_link_state_change(res_softc->sc_ifp);
					} else {
						printf("Error, ifp not referenced\n");
					}
				}
				splx(s);

				printf("Rescheduling heartattack\n");
				rw_enter_write(&eou_softc_lock);
				struct timeout *heartattack = &(res_softc->timer_heartattack);
				timeout_add_sec(heartattack, PING_HEARTATTACK);
				rw_exit_write(&eou_softc_lock);
			}

			break;

		case EOU_T_DATA:
			printf("__DATA__\n");
			if (res_softc != NULL) {
				//printf("Before split[%d]\n", mp0->m_pkthdr.len);
				struct mbuf *split_m = m_split(mp0, 6, M_WAITOK);
				//printf("After split[%d] | [%d]\n", split_m->m_pkthdr.len,mp0->m_pkthdr.len);

				struct mbuf_list *mp_list = malloc(sizeof(struct mbuf_list), 
					M_SUBPROC, M_WAITOK | M_ZERO);
				ml_init(mp_list);
				ml_enqueue(mp_list, split_m);			
				if_input(res_softc->sc_ifp, mp_list);
				
			}

			break;
		
		default:
			printf("Unknown data received\n");
	}
}

/* Ping for the interface  */
void 
eou_softc_heartbeat(void* data)
{
	//printf("__Heartbeat__\n");

	struct ifnet 		*ifp = (struct ifnet*) data;
	struct eou_softc	*sc = (struct eou_softc *) ifp->if_softc;
	int error = 0;

	eou_softc_ping(ifp);
	timeout_add_sec(&sc->timer_heartbeat, PING_HEARTBEAT);

	if (error) {
		printf("Heartbeat Error [%d]\n", error);
	}
}

void 
eou_softc_heartattack(void* data)
{
	printf("__Heartattack!__\n");

	struct ifnet 		*ifp = (struct ifnet*) data;
	struct eou_softc	*sc = (struct eou_softc *) ifp->if_softc;
	int error = 0;

	/* Bring it down */
	ifp->if_link_state = LINK_STATE_DOWN;
	if_link_state_change(ifp);

	timeout_del(&sc->timer_heartbeat);
	if (error) {
		printf("Heartcheck Error [%d]\n", error);
	}
}






